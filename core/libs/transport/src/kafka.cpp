#include <boost/asio/require_concept.hpp>
#include <boost/smart_ptr/make_shared_object.hpp>
#include <transport/kafka.h>
#include <utils/utils.h>
#include <librdkafka/rdkafka.h>
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <signal/signal.h>

struct KafkaProducer::Impl {
    explicit Impl(SPSQ<Signal>& ewma,
        SPSQ<Signal>& zscore,
        SPSQ<Signal>& beta,
        SPSQ<Signal>& corr)
    : ewma_(ewma), zscore_(zscore)
        , beta_(beta), corr_(corr)
    {}

    void run() {
        Signal* slot = nullptr;

        while (running_.load(std::memory_order_relaxed)) {
            if ((slot = ewma_.front())) {
                produce(*slot);
                ewma_.pop();
                continue;
            }

            if ((slot = zscore_.front())) {
                produce(*slot);
                zscore_.pop();
                continue;
            }

            if ((slot = beta_.front())) {
                produce(*slot);
                beta_.pop();
                continue;
            }

            if ((slot = corr_.front())) {
                produce(*slot);
                corr_.pop();
                continue;
            }
        }

        utils::spin_pause();
    }

    void stop() {running_.store(false, std::memory_order_relaxed);};
private:
    SPSQ<Signal>& ewma_;
    SPSQ<Signal>& zscore_;
    SPSQ<Signal>& beta_;
    SPSQ<Signal>& corr_;

    rd_kafka_t* rk_ = nullptr;

    std::atomic<bool> running_;
    char ser_buf_[512];

    uint64_t produced_ = 0;
    uint64_t errors_ = 0;



    static constexpr const char* BROKERS = "localhost:9092";
    static constexpr const char* TOPIC = "signals";

    int serialize(const Signal& sig,
        char* buf,
        std::size_t buf_size) noexcept {

        const char* type_str = [&]() noexcept -> const char* {
            switch (sig.type) {
            case SigType::EWMA_CROSS: return "EWMA_CROSS";
            case SigType::ZSCORE: return "ZSCORE";
            case SigType::BETA: return "BETA";
            case SigType::CORRELATION: return "CORRELATION";
            default: return "UNKNOWN";
            }
            }();

        const char* dir_str = [&]() noexcept -> const char* {
            switch (sig.direction) {
            case Direction::BUY: return "BUY";
            case Direction::SELL: return "SELL";
            case Direction::NEUTRAL: return "NEUTRAL";
            default: return "NEUTRAL";
            }
            }();

        const char* sym_str = [&]() noexcept -> const char* {
            switch (sig.symbol) {
            case SigSymbol::BTCUSDT: return "BTCUSDT";
            case SigSymbol::ETHUSDT: return "ETHUSDT";
            default: return "UNKNOWN";
            }
            }();

        return std::snprintf(buf, buf_size,
            "{"
            "\"ts\":%lld\","
            "\"sym\":\"%s\","
            "\"type\":\"%s\","
            "\"dir\":\"%s\","
            "\"val\":%.8f"
            "}",
            static_cast<long long>(sig.timestamp),
            sym_str,
            type_str,
            dir_str,
            sig.value
        );
    }

    //partitions 0,1,2
    int32_t partition_for(const Signal& sig) noexcept {
        if (sig.symbol == SigSymbol::BTCUSDT) {
            return 0;
        }

        if (sig.symbol == SigSymbol::ETHUSDT) {
            return 1;
        }

        return 2;
    }

    rd_kafka_resp_err_t try_produce(int32_t partition,
                                    SigSymbol key,
                                    std::size_t key_len,
                                    std::size_t val_len) {
        return rd_kafka_producev(
            rk_,
            RD_KAFKA_V_TOPIC(TOPIC),
            RD_KAFKA_V_PARTITION(partition),
            RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_COPY),
            RD_KAFKA_V_KEY(&key, key_len),
            RD_KAFKA_V_OPAQUE(nullptr),
            RD_KAFKA_V_END
        );
    }

    void produce(const Signal& sig) {
        const int len = serialize(sig, ser_buf_, sizeof(ser_buf_));
        if (len <= 0 || len >= static_cast<int>(sizeof(ser_buf_))) {
            ++errors_;
            return;
        }

        const int32_t partition = partition_for(sig);
        const size_t key_len = sizeof(sig.symbol);

        auto err = try_produce(partition, sig.symbol, key_len, len);

        if (err == RD_KAFKA_RESP_ERR__QUEUE_FULL) {
            rd_kafka_poll(rk_, 10);
            err = try_produce(partition, sig.symbol, key_len, len);
        }

        if (err != RD_KAFKA_RESP_ERR_NO_ERROR) {
            std::fprintf(stderr, "[kafka] produce: %s\n",
                rd_kafka_err2str(err));
            ++errors_;
            return;
        }


        ++produced_;
        if ((produced_ & 63) == 0) rd_kafka_poll(rk_, 0);
    }

};

KafkaProducer::KafkaProducer(SPSQ<Signal>& ewma,
    SPSQ<Signal>& zscore,
    SPSQ<Signal>& beta,
    SPSQ<Signal>& corr)
 : impl_(std::make_shared<Impl>(ewma, zscore, beta, corr))
{}

void KafkaProducer::run() {impl_->run();}
void KafkaProducer::stop() {impl_->stop();}

KafkaProducer::~KafkaProducer() {KafkaProducer::stop();}
