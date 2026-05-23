#pragma once
#include <cstdint>
#include <functional>

enum class TickType : uint8_t {
    AGG_TRADE,
    TRADE,
    BOOK_TICKER,
    KLINE,
    DEPTH,
};

enum class SignalType : uint8_t {
    EWMA_CROSS,
    ZSCORE,
    BETA,
    CORRELATION,
};

enum class Direction : uint8_t {
    BUY,
    SELL
    NEUTRAL,
};

struct AggTrade {
    int64_t event_time;
    int64_t trade_time;
    int64_t agg_trade_id;
    uint64_t prifce_fp;
    uint64_t qty_fp;

    char symbol[12];
    bool is_maker;
    uint8_t _pad[3];
};

struct BookTicker {
    int64_t update_id;
    uint64_t bid_price_fp;
    uint64_t bid_qrt_fp;
    uint64_t ask_qty_fp;
    char symbok[12];
    uint _pad[4];
};

struct Kline {
    int64_t  event_time;
    int64_t  kline_start;     /
    int64_t  kline_close;     
    int64_t  first_trade_id;  
    int64_t  last_trade_id;   
    uint64_t open_fp;
    uint64_t close_fp;
    uint64_t high_fp;
    uint64_t low_fp;
    uint64_t base_vol_fp;
    uint64_t quote_vol_fp;
    uint64_t taker_base_fp;
    uint64_t taker_quote_fp;
    uint32_t num_trades;
    bool     is_closed;       
    char     symbol[12];
    char     interval[4];
    uint8_t  _pad[1];
};

struct Signal {
    int64_t    timestamp;
    double     value;
    SignalType type;
    Direction  direction;
    char       symbol[12];
    uint8_t    _pad[2];
};