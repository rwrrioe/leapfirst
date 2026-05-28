#include <signal/engine.h>
#include <utils/utils.h>
#include <workers/beta.h>
#include <workers/correlation.h>
#include <transport/websocket_client.h>
#include <signal/tick.h>
#include <signal/signal.h>
#include <queue/spsq.h>
#include <workers/ewma.h>
#include <workers/zscore.h>
#include <thread>

using json = simdjson::ondemand::object;

struct Engine::Impl {
public:
    explicit Impl()
        :ewma_worker_(btc_to_ewma_, ewma_signals_)
        , zscore_worker_(btc_to_zscore_, zscore_signals_)
        , beta_worker_(to_beta_, beta_signals_)
        , corr_worker_(to_corr_, corr_signals_)
    {
        ws_client_ = std::make_unique<WebSocketClient>(
            make_config(),
            [this](std::string_view stream, json& data) {
                route(stream, data);
            }
        );
    }

    void run() {
        threads_.emplace_back([this] {
            std::printf("[ENGINE] ewma thread started\n"); fflush(stdout);
            corepin::pin_to_core(2); ewma_worker_.run();
            });
        threads_.emplace_back([this] {
            std::printf("[ENGINE] zscore thread started\n"); fflush(stdout);
            corepin::pin_to_core(4); 
            zscore_worker_.run();});

        threads_.emplace_back([this] {
            std::printf("[ENGINE] beta thread started\n"); fflush(stdout);
            corepin::pin_to_core(6); 
            beta_worker_.run();
            });

        threads_.emplace_back([this] {
            std::printf("[ENGINE] beta thread started\n"); fflush(stdout);
            corepin::pin_to_core(8); 
            corr_worker_.run();});

#ifndef NDEBUG
        threads_.emplace_back([this] {debug_drain();});
#endif // !NDEBUG


        corepin::pin_to_core(10);
        ws_client_->run();
    }

    void stop() {
        ws_client_->stop();

        ewma_worker_.stop();
        zscore_worker_.stop();
        beta_worker_.stop();
        corr_worker_.stop();

        for (auto& t : threads_) {
            if (t.joinable()) t.join();
        }
    }
private:
    std::unique_ptr<WebSocketClient> ws_client_;

    SPSQ<AggTrade> btc_to_ewma_{ 4096 };
    SPSQ<AggTrade> btc_to_zscore_{ 4096 };
    //beta, corr require two values (btc, eth)
    SPSQ<AggTrade> to_beta_{ 4096 };
    SPSQ<AggTrade> to_corr_{ 4096 };

    //TODO
    //add book, kline

    SPSQ<Signal> ewma_signals_{ 1024 };
    SPSQ<Signal> zscore_signals_{ 1024 };
    SPSQ<Signal> beta_signals_{ 1024 };
    SPSQ<Signal> corr_signals_{ 1024 };


    EwmaWorker ewma_worker_{ btc_to_ewma_, ewma_signals_ };
    ZScoreWorker zscore_worker_{ btc_to_zscore_, zscore_signals_ };
    BetaWorker beta_worker_{ to_beta_, beta_signals_ };
    CorrelationWorker corr_worker_{ to_corr_, corr_signals_ };

    std::vector<std::thread> threads_;
    std::atomic<bool> is_stopped_{ false };

    //routers
    void route(std::string_view stream,
        json& data) {

        if (stream.ends_with("@aggTrade")) { route_agg_trade(data); return; }
        if (stream.ends_with("@bookTicker")) { route_book(data); return; }
        if (stream.ends_with("@kline")) { route_kline(data); return; }
    }

    void route_agg_trade(json& data) {
        const AggTrade t = parse_agg_trade(data);

 /*       std::printf("[RAW] symbol=%d price=%.8f qty=%.8f\n",
            (int)t.symbol,
            fp8_to_double(t.price_fp),
            fp8_to_double(t.qty_fp));*/


        switch (t.symbol) {
        case TickSymbol::BTCUSDT: {
            bool ok = btc_to_ewma_.try_emplace(t);
            btc_to_zscore_.try_emplace(t);
            to_beta_.try_emplace(t);
            to_corr_.try_emplace(t);
            break;
        }
        case TickSymbol::ETHUSDT:{
            to_beta_.try_emplace(t);
            to_corr_.try_emplace(t);
            break;
        }
        default:break;
        }
    }

    void route_book(json& data) {}
    void route_kline(json& data) {}

    //parsers

    AggTrade parse_agg_trade(json& data) {
        AggTrade t{};

        std::string_view skip, s, p, q;
        data["e"].get(skip);
        data["E"].get(t.event_time);
        data["s"].get(s);
        data["a"].get(t.agg_trade_id);
        data["p"].get(p);
        data["q"].get(q);
        data["T"].get(t.trade_time);
        data["m"].get(t.is_maker);

        t.price_fp = parse_fp8(p);
        t.qty_fp = parse_fp8(q);
        t.symbol = classify_symbol(s);

        return t;
    }

    BookTicker parse_book_ticker(json& data) {}

    Kline parse_kline(json& data) {}

    TickSymbol classify_symbol(std::string_view sym) noexcept {
        if (sym == "BTCUSDT") return TickSymbol::BTCUSDT;
        if (sym == "ETHUSDT") return TickSymbol::ETHUSDT;
        return TickSymbol::UNKNOWN;
    }


    //dev
    WebSocketClient::Config make_config() {
        WebSocketClient::Config cfg;
        cfg.host = "stream.binance.com";
        cfg.port = "9443";
        cfg.streams = std::vector<std::string>{ "btcusdt@aggTrade", "ethusdt@aggTrade", "btcusdt@bookTicker", "btcusdt@kline_1m" };
        return cfg;
    }


#ifndef NDEBUG
    void debug_drain() {
        while (!is_stopped_.load(std::memory_order_relaxed)) {
            bool got_any = false;
            Signal* s;
            if ((s = ewma_signals_.front())) { std::printf("[EWMA  ] %.8f\n", s->value);  fflush(stdout); ewma_signals_.pop();   got_any = true; }
            if ((s = zscore_signals_.front())) { std::printf("[ZSCORE] %.8f\n", s->value);  fflush(stdout); zscore_signals_.pop(); got_any = true; }
            if ((s = beta_signals_.front())) { std::printf("[BETA  ] %.8f\n", s->value);  fflush(stdout); beta_signals_.pop();   got_any = true; }
            if ((s = corr_signals_.front())) { std::printf("[CORR  ] %.8f\n", s->value);  fflush(stdout); corr_signals_.pop();   got_any = true; }
            if (!got_any) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
#endif // !NDEBUG

   };



Engine::Engine()
       : impl_(std::make_shared<Impl>())
{}

void Engine::run() { impl_->run(); }
void Engine::stop() { impl_->stop(); }

Engine::~Engine() { Engine::stop(); }
