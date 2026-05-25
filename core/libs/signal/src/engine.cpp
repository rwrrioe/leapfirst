#include <transport/websocket_client.h>
#include <signal/tick.h>
#include <signal/signal.h>
#include <queue/spsq.h>
#include <workers/ewma.h>
#include <workers/zscore.h>
#include <thread>

class Engine {
public:
private:
    SPSQ<AggTrade> to_ewma_;
    SPSQ<AggTrade> to_zscore_;
    SPSQ<AggTrade> to_beta_btc_;
    SPSQ<AggTrade> to_beta_eth_;
    SPSQ<BookTicker> to_book_;
    SPSQ<Signal> signal_out_;

    EwmaWorker   ewma_worker_ {to_ewma_, signal_out_};
    ZScoreWorker zscore_worker_ { to_zscore_,  signal_out_ };

    std::unique_ptr<WebSocketClient> ws_client_;
    std::vector<std::thread> threads_;
};