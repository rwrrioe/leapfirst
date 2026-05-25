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

struct AggTrade {
    int64_t event_time;
    int64_t trade_time;
    int64_t agg_trade_id;
    uint64_t price_fp;
    uint64_t qty_fp;

    char symbol[12];
    bool is_maker;
    uint8_t _pad[3];
};

struct BookTicker {
    int64_t update_id;
    uint64_t bid_price_fp;
    uint64_t bid_qty_fp;
    uint64_t ask_qty_fp;
    uint64_t ask_price_fp;
    char symbok[12];
    uint8_t _pad[4];
};

struct Kline {
    int64_t  event_time;
    int64_t  kline_start;
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