#include <workers/zscore.h>

#include <immintrin.h>
#include <csignal>
#undef signal         
#include <cmath>
#include <atomic>
#include <thread>

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
                spin_pause();
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
            }
        }
    }


    void stop() {
        running_.store(false, std::memory_order_relaxed);
    }

private:
    static void spin_pause() noexcept {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__)
        _mm_pause();            
#elif defined(__aarch64__) || defined(_M_ARM64)
        asm volatile("yield" ::: "memory");
#else
        std::this_thread::yield();
#endif
    }

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