#include <workers/ewma.h>
#include <utils/utils.h>

#include <csignal>
#undef signal
#include <cmath>
#include <atomic>

#include <queue/spsq.h>
#include <signal/signal.h>
#include <signal/tick.h>
#include <transport/types.h>


struct EwmaWorker::Impl {
	explicit Impl (SPSQ<AggTrade>& in,
			SPSQ<Signal>& out)
		:in_(in), out_(out) {}

	void run() {

		while (running_.load(std::memory_order_relaxed)) {
			auto tick = in_.front();
			if (!tick) {
				utils::spin_pause();
				continue;
			}

			//calculate
			const double price = fp8_to_double(tick->price_fp);
			ema_fast_ = ALPHA_FAST * price + (1.0 - ALPHA_FAST) * ema_fast_;
			ema_slow_ = ALPHA_SLOW * price + (1.0 - ALPHA_SLOW) * ema_slow_;
			const int64_t evtime = tick->event_time;

			in_.pop();
			const double macd = ema_fast_ - ema_slow_;

			if (std::abs(macd) > THRESHOLD) {
				Signal sig{};
				sig.timestamp = evtime;
				sig.value = macd;
				sig.type = SigType::EWMA_CROSS;
				sig.direction = (macd > 0) ? Direction::BUY : Direction::SELL;

				out_.try_emplace(sig);
			}
		}


	}


	void stop() {
		running_.store(false, std::memory_order_relaxed);
	}

private:
	SPSQ<AggTrade>& in_;
	SPSQ <Signal>& out_;
	std::atomic<bool> running_{ true };
	double ema_fast_ = 0.0, ema_slow_ = 0.0;

	static constexpr double ALPHA_FAST = 2.0 / (12 + 1);
	static constexpr double ALPHA_SLOW = 2.0 / (26 + 1);
	static constexpr double THRESHOLD = 0.5;
};

EwmaWorker::EwmaWorker(SPSQ<AggTrade>& in,
	SPSQ<Signal>& out)
	: impl_(std::make_shared<Impl>(in, out))
{}

EwmaWorker::~EwmaWorker() {impl_->stop();}

void EwmaWorker::run() { impl_->run(); }
void EwmaWorker::stop() { impl_->stop(); }
