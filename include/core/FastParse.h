#pragma once
#include <cstdint>
#include <cstddef>

/*
    Minimal, allocation-free numeric parsing for the market-data hot path.

    std::stod allocates a std::string, consults the C locale, and can throw.
    std::from_chars would be the right answer but libc++ still deletes the
    floating-point overload, so we parse the fixed decimal format Binance
    actually sends ("63450.12000000") directly out of the read buffer.

    Assumes plain decimals: optional sign, digits, optional '.', digits.
    No exponent, no NaN/Inf. Returns false on anything else so the caller
    can fall back to a general parser.
*/
namespace fastparse {

// 1e0 .. 1e22 are all exactly representable as doubles.
inline constexpr double kPow10[] = {
    1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,  1e8,  1e9,  1e10, 1e11,
    1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22
};
inline constexpr int kMaxScale = 22;
inline constexpr int kMaxDigits = 19;   // fits in uint64_t

inline bool parseDecimal(const char* b, const char* e, double& out) {
    if (b >= e) return false;

    bool neg = false;
    if (*b == '-' || *b == '+') { neg = (*b == '-'); ++b; }

    uint64_t mantissa = 0;
    int digits = 0;
    int scale = 0;
    bool seenDot = false;
    bool seenDigit = false;

    for (const char* p = b; p < e; ++p) {
        const char c = *p;
        if (c == '.') {
            if (seenDot) return false;
            seenDot = true;
            continue;
        }
        if (c < '0' || c > '9') return false;
        seenDigit = true;
        if (digits == kMaxDigits) return false;   // caller falls back
        mantissa = mantissa * 10 + static_cast<uint64_t>(c - '0');
        ++digits;
        if (seenDot) ++scale;
    }

    if (!seenDigit || scale > kMaxScale) return false;

    double v = static_cast<double>(mantissa) / kPow10[scale];
    out = neg ? -v : v;
    return true;
}

inline bool parseInt(const char* b, const char* e, long long& out) {
    if (b >= e) return false;

    bool neg = false;
    if (*b == '-' || *b == '+') { neg = (*b == '-'); ++b; }
    if (b >= e) return false;

    unsigned long long v = 0;
    for (const char* p = b; p < e; ++p) {
        const char c = *p;
        if (c < '0' || c > '9') return false;
        if (v > (0x7FFFFFFFFFFFFFFFULL - static_cast<unsigned long long>(c - '0')) / 10ULL) return false;
        v = v * 10 + static_cast<unsigned long long>(c - '0');
    }
    out = neg ? -static_cast<long long>(v) : static_cast<long long>(v);
    return true;
}

} // namespace fastparse
