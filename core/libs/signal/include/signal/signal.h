#pragma once
#include <cstdint>

enum class Direction : uint8_t {
    BUY,
    SELL,
    NEUTRAL,
};

enum class SigType : uint8_t {
    EWMA_CROSS,
    ZSCORE,
    BETA,
    CORRELATION,
};

enum class SigSymbol : uint8_t {
    BTCUSDT,
    ETHUSDT
};

struct Signal {
    int64_t    timestamp;
    double     value;
    SigType type;
    Direction  direction;
    SigSymbol symbol;
    uint8_t    _pad[5];
};
