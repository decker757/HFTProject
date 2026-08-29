#pragma once
#include "market_data/Trade.h"

enum class ParseMode {
    Fast,   // hand-rolled scan over the raw read buffer, no allocation
    Json    // nlohmann DOM parse, kept for A/B benchmarking and validation
};

// Both parse a Binance <symbol>@trade payload from [begin, end).
// recv_ns is left untouched; the caller stamps it.
bool parseTradeFast(const char* begin, const char* end, Trade& out);
bool parseTradeJson(const char* begin, const char* end, Trade& out);

// Verbatim reproduction of the original hot path: copy the buffer into a
// std::string, DOM-parse it, then pull each number out through another
// std::string copy and std::stod. Kept only so the benchmark compares against
// what the code actually used to do.
bool parseTradeLegacy(const char* begin, const char* end, Trade& out);

inline bool parseTrade(const char* begin, const char* end, Trade& out, ParseMode mode) {
    return mode == ParseMode::Fast ? parseTradeFast(begin, end, out)
                                   : parseTradeJson(begin, end, out);
}
