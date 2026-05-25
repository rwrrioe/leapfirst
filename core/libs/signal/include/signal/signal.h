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


struct Signal {
    int64_t    timestamp;
    double     value;
    SigType type;
    Direction  direction;
    char       symbol[12];
    uint8_t    _pad[2];
};
