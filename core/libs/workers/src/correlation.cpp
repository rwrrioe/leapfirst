#include <workers/correlation.h>
#include <utils/utils.h>
#include <transport/types.h>

struct CorrelationWorker::Impl {
	static constexpr std::size_t WINDOW = 256;

	explicit Impl(SPSQ<AggTrade>& in,
				  SPSQ<Signal>& out)
		: in_(in), out_(out)
	{}
	
	void run() {

		while (running_.load(std::memory_order_relaxed)) {
			AggTrade* slot = in_.front();

			if (!slot) {
				utils::spin_pause();
				continue;
			}

			const AggTrade tick = *slot;
			in_.pop();

			const double price = fp8_to_double(tick.price_fp);

			switch (tick.symbol) {
			case TickSymbol::BTCUSDT: {
				const double ret = last_btc_ > 0.0
					? std::log(price / last_btc_) : 0.0;
				last_btc_ = price;
				update_btc(ret);
				break;
			}
			case TickSymbol::ETHUSDT: {
				const double ret = last_eth_ > 0.0
					? std::log(price / last_eth_) : 0.0;
				last_eth_ = price;
				update_eth(ret);
				break;
			}
			default: break;
			}

			if (n_btc_ >= WINDOW && n_eth_ >= WINDOW) {
				compute(tick.event_time);
			}
		}
	}
	
	void stop() {

	}
private:
	double btc_buf_[WINDOW]{};
	double eth_buf_[WINDOW]{};

	SPSQ<AggTrade>& in_;
	SPSQ<Signal>& out_;
	std::atomic<bool> running_{ true };

	static constexpr double HIGH_CORR = 0.8;
	static constexpr double LOW_CORR = -0.8;

	std::size_t head_btc_ = 0;
	std::size_t head_eth_ = 0;
	std::size_t n_btc_ = 0;
	std::size_t n_eth_ = 0;

	double last_btc_ = 0.0;
	double last_eth_ = 0.0;

	double sum_x_ = 0.0;
	double sum_y_ = 0.0;
	double sum_x2_ = 0.0;
	double sum_y2_ = 0.0;
	double sum_xy_ = 0.0;


	void update_btc(double ret) {
		const double old = btc_buf_[head_btc_];
		sum_x_ -= old;
		sum_x2_ -= old * old;
		sum_xy_ -= old * eth_buf_[head_btc_];

		btc_buf_[head_btc_] = ret;
		sum_x_ += ret;
		sum_x2_ += ret * ret;
		sum_xy_ += ret * eth_buf_[head_btc_];

		head_btc_ = (head_btc_ + 1) & (WINDOW - 1);
		if (n_btc_ < WINDOW) ++n_btc_;
	}

	void update_eth(double ret) {
		const double old = eth_buf_[head_eth_];
		sum_y_ -= old;
		sum_y2_ -= old * old;
		sum_xy_ -= btc_buf_[head_eth_] * old;

		eth_buf_[head_eth_] = ret;
		sum_y_ += ret;
		sum_y2_ += ret * ret;
		sum_xy_ += btc_buf_[head_eth_] * ret;

		head_eth_ = (head_eth_ + 1) & (WINDOW - 1);
		if (n_eth_ < WINDOW) ++n_eth_;
	}

	void compute(int64_t timestamp) {
		const double N = static_cast<double>(WINDOW);
		const double mean_x = sum_x_ / N;
		const double mean_y = sum_y_ / N;
		const double var_x = sum_x2_ / N - mean_x * mean_x;
		const double var_y = sum_y2_ / N - mean_y * mean_y;

		if (var_x < 1e-12 || var_y < 1e-12) return;

		const double cov = sum_xy_ / N - mean_x * mean_y;
		const double corr = cov / std::sqrt(var_x * var_y);

		if (corr > HIGH_CORR || corr < LOW_CORR) {
			Signal sig{};
			sig.timestamp = timestamp;
			sig.value = corr;
			sig.type = SigType::CORRELATION;
			sig.direction = corr > 0.0 ? Direction::BUY : Direction::SELL;
			sig.symbol = SigSymbol::BTCUSDT;

			out_.try_emplace(sig);
		}
	}

};

CorrelationWorker::CorrelationWorker(SPSQ<AggTrade>& in, SPSQ<Signal>& out) 
	: impl_(std::make_shared<Impl>(in, out))
{}

void CorrelationWorker::run() { impl_->run(); }
void CorrelationWorker::stop() { impl_->stop(); }

CorrelationWorker::~CorrelationWorker() { CorrelationWorker::stop(); }