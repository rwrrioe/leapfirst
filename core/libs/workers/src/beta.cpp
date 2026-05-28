#include <workers/beta.h>
#include <utils/utils.h>
#include <transport/types.h>
#include <atomic>

struct BetaWorker::Impl {
	static constexpr std::size_t WINDOW = 256;

	explicit Impl (SPSQ<AggTrade>& in,
				   SPSQ<Signal>& out)
		: in_(in)
		, out_(out)
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
				push_btc(ret);
				break;
			}
			case TickSymbol::ETHUSDT: {
				const double ret = last_eth_ > 0.0
					? std::log(price / last_eth_) : 0.0;

				last_eth_ = price;
				push_eth(ret);
				break;
			}
			default: break;
			}

			if (n_ >= WINDOW) {
				compute(tick.event_time);
			}
		}
	}

	void stop() { running_.store(false, std::memory_order_relaxed); }

private:

	double btc_buf_[WINDOW]{};
	double eth_buf_[WINDOW]{};

	std::size_t btc_head_ = 0;
	std::size_t eth_head_ = 0;
	std::size_t n_ = 0;

	double last_btc_ = 0.0;
	double last_eth_ = 0.0;

	double sum_btc_ = 0.0;
	double sum_eth_ = 0.0;

	//sum(btc^2), sum(eth^2)
	double sum_btc2_ = 0.0;
	double sum_eth2_ = 0.0;

	//sum(btc*eth)
	double sum_btceth_ = 0.0;
	SPSQ<AggTrade>& in_;
	SPSQ<Signal>& out_;
	std::atomic<bool> running_{ true };

	static constexpr double THRESHOLD = 0.05;


	void push_btc(double ret) {
		const double old = btc_buf_[btc_head_];
		sum_btc_ -= old;
		sum_btc2_ -= old * old;
		sum_btceth_ -= old * eth_buf_[btc_head_];

		btc_buf_[btc_head_] = ret;
		sum_btc_ += ret;
		sum_btc2_ += ret * ret;
		sum_btceth_ += ret * eth_buf_[btc_head_];

		btc_head_ = (btc_head_ + 1) & (WINDOW - 1);
		if (n_ < WINDOW) ++n_;
	}

	void push_eth(double ret) {
		const double old = eth_buf_[eth_head_];
		sum_eth_ -= old;
		sum_eth2_ -= old * old;
		sum_btceth_ -= old * eth_buf_[eth_head_];

		eth_buf_[eth_head_] = ret;
		sum_eth_ += ret;
		sum_eth2_ += ret * ret;
		sum_btceth_ += ret * eth_buf_[eth_head_];

		eth_head_ = (eth_head_ + 1) & (WINDOW - 1);
		if (n_ < WINDOW) ++n_;
	}

	void compute(int64_t timestamp) {
		const double N = static_cast<double>(WINDOW);
		const double mean_btc = sum_btc_ / N;
		const double mean_eth = sum_eth_ / N;
		const double cov = sum_btceth_ / N - mean_btc * mean_eth;
		const double var_eth = sum_eth2_ / N - mean_btc * mean_eth;
		
		if (var_eth < 1e-12) return;

		const double beta = cov / var_eth;

		if (std::abs(beta - 1.0) > THRESHOLD) {
			Signal sig{};
			sig.timestamp = timestamp;
			sig.value = beta;
			sig.type = SigType::BETA;
			sig.direction = beta > 1.0 ? Direction::BUY : Direction::SELL;
			sig.symbol = SigSymbol::BTCUSDT;

			out_.try_emplace(sig);
		}
	}
};

BetaWorker::BetaWorker(SPSQ<AggTrade>& in,SPSQ<Signal>& out) 
	: impl_(std::make_shared<Impl>(in, out))
{}

void BetaWorker::run() { impl_->run(); }
void BetaWorker::stop() { impl_->stop(); }
BetaWorker::~BetaWorker() { BetaWorker::stop(); }