#pragma once
#include <string_view>
#include <simdjson.h>
#include <functional>

using DispatchCallback = std::function<
    void(std::string_view stream,
        simdjson::ondemand::object& data)
>;

inline uint64_t parse_fp8(std::string_view s) noexcept {
    uint64_t integer = 0, frac = 0;
    std::size_t frac_digits = 0;

    const char* ptr = s.data();
    const char* end = s.data() + s.size();

    while (ptr < end && *ptr != '.')
        integer = integer * 10 + (*ptr++ - '0');

    if (ptr < end && *ptr == '.') {
        ++ptr;
        while (ptr < end && frac_digits < 8) {
            frac = frac * 10 + (*ptr++ - '0');
            ++frac_digits;
        }
    }

    while (frac_digits++ < 8) frac *= 10;

    return integer * 100'000'000ULL + frac;
}


inline double fp8_to_double(uint64_t fp) noexcept {
    return static_cast<double>(fp) * 1e-8;
}

inline bool symbol_is(const char* sym, std::string_view target) noexcept {
    return std::string_view{ sym, target.size() } == target;
}

inline std::string fp8_to_string(uint64_t fp) noexcept {
    auto integer = fp / 100'000'000ULL;
    auto frac = fp % 100'000'000ULL;

    char buf[32];
    auto n = std::snprintf(buf, sizeof(buf), "%llu.%08llu", integer, frac);
    return { buf, static_cast<std::size_t>(n) };
}