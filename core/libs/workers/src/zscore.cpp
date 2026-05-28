#include <workers/zscore.h>
#include <utils/utils.h>

#include <csignal>
#undef signal         
#include <cmath>
#include <atomic>

#include <queue/spsq.h>
#include <signal/signal.h>  
#include <signal/tick.h>
#include <transport/types.h>

struct ZScoreWorker::Impl {
    explicit Impl(SPSQ<AggTrade>& in, SPSQ<Signal>& out) : in_(in), out_(out) {}

    void run() {

        while (running_.load(std::memory_order_relaxed)) {
            auto* tick = in_.front();
            if (!tick) {
                utils::spin_pause();
                continue;
            }
    
            //calculate
            const double p = fp8_to_double(tick->price_fp);
            ++n_;

            const double delta = p - mean_;
            mean_ += delta / n_;

            const double delta2 = p - mean_;
            m2_ += delta * delta2;
            const uint64_t evtime = tick->event_time;

            in_.pop();

            if (n_ < WIN) continue;

            const double stddev = std::sqrt(m2_ / n_);
            if (stddev < 1e-9) continue;
            
            const double z = (p - mean_) / stddev;
            if (std::abs(z) > THRESHOLD) {
                Signal sig{};
                sig.timestamp = evtime;
                sig.value = z;
                sig.type = SigType::ZSCORE;
                sig.direction = z > 0 ? Direction::SELL : Direction::BUY;
                out_.try_emplace(sig);
            }
        }
    }


    void stop() {
        running_.store(false, std::memory_order_relaxed);
    }

private:
    uint64_t n_ = 0;
    double mean_ = 0.0, m2_ = 0.0;
    
    SPSQ<AggTrade>& in_;
    SPSQ<Signal>& out_;
    std::atomic<bool> running_{ true };

    static constexpr uint64_t WIN = 100;
    static constexpr double THRESHOLD = 2.0;
};

ZScoreWorker::ZScoreWorker(SPSQ<AggTrade>& in, SPSQ<Signal>& out)
    : impl_(std::make_shared<Impl>(in, out))
{}

ZScoreWorker::~ZScoreWorker() {
    impl_->stop();
}

void ZScoreWorker::run() { impl_->run(); }
void ZScoreWorker::stop() { impl_->stop(); }