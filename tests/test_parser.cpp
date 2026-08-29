#include "core/FastParse.h"
#include "exchange/TradeParser.h"
#include "market_data/Trade.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

static int failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);    \
            ++failures;                                                    \
        }                                                                  \
    } while (0)

static void checkDecimal(const char* s) {
    double got = 0;
    const bool ok = fastparse::parseDecimal(s, s + std::strlen(s), got);
    const double want = std::strtod(s, nullptr);
    CHECK(ok);
    if (ok) {
        // Must match strtod to within a rounding error.
        const double tol = std::fabs(want) * 1e-15 + 1e-18;
        if (std::fabs(got - want) > tol) {
            std::printf("FAIL decimal %s -> %.17g, strtod says %.17g\n", s, got, want);
            ++failures;
        }
    }
}

static void testDecimals() {
    checkDecimal("0");
    checkDecimal("1");
    checkDecimal("63450.12000000");
    checkDecimal("0.00000001");
    checkDecimal("-12.5");
    checkDecimal("99999999.99999999");
    checkDecimal("100");
    checkDecimal(".5");
    checkDecimal("5.");

    double d = 0;
    CHECK(!fastparse::parseDecimal("", "", d));
    CHECK(!fastparse::parseDecimal("abc", "abc" + 3, d));
    CHECK(!fastparse::parseDecimal("1.2.3", "1.2.3" + 5, d));
    CHECK(!fastparse::parseDecimal("1e5", "1e5" + 3, d));   // no exponent support: must reject, not guess
    CHECK(!fastparse::parseDecimal("-", "-" + 1, d));
}

static void testInts() {
    long long v = 0;
    CHECK(fastparse::parseInt("0", "0" + 1, v) && v == 0);
    CHECK(fastparse::parseInt("1746000000121", "1746000000121" + 13, v) && v == 1746000000121LL);
    CHECK(fastparse::parseInt("-42", "-42" + 3, v) && v == -42);
    CHECK(!fastparse::parseInt("12a", "12a" + 3, v));
    CHECK(!fastparse::parseInt("", "", v));
    // Overflow must be rejected rather than silently wrapping.
    const char* huge = "99999999999999999999999";
    CHECK(!fastparse::parseInt(huge, huge + std::strlen(huge), v));
}

static void expectAgreement(const std::string& payload) {
    const char* b = payload.data();
    const char* e = b + payload.size();

    Trade fast, json, legacy;
    const bool okF = parseTradeFast(b, e, fast);
    const bool okJ = parseTradeJson(b, e, json);
    const bool okL = parseTradeLegacy(b, e, legacy);

    CHECK(okF == okJ);
    CHECK(okJ == okL);
    if (!okF || !okJ) return;

    CHECK(std::strcmp(fast.symbol, json.symbol) == 0);
    CHECK(fast.price == json.price);
    CHECK(fast.quantity == json.quantity);
    CHECK(fast.timestamp == json.timestamp);
    CHECK(fast.is_sell == json.is_sell);
}

static void testPayloads() {
    expectAgreement(
        R"({"e":"trade","E":1746000000123,"s":"BTCUSDT","t":4738291,"p":"63450.12000000",)"
        R"("q":"0.00134000","T":1746000000121,"m":true,"M":true})");

    // is_sell false, different symbol length, small price
    expectAgreement(
        R"({"e":"trade","E":1746000000999,"s":"XRPUSDT","t":88,"p":"0.51230000",)"
        R"("q":"1200.00000000","T":1746000000998,"m":false,"M":true})");

    // Longest symbol we allow
    expectAgreement(
        R"({"e":"trade","E":1,"s":"1000SATSUSDT","t":1,"p":"1.00000000",)"
        R"("q":"1.00000000","T":2,"m":true,"M":true})");

    // Malformed input must be rejected, not half-parsed.
    Trade t;
    const std::string truncated = R"({"e":"trade","s":"BTCUSDT","p":"634)";
    CHECK(!parseTradeFast(truncated.data(), truncated.data() + truncated.size(), t));

    const std::string missingFields = R"({"e":"trade","s":"BTCUSDT"})";
    CHECK(!parseTradeFast(missingFields.data(), missingFields.data() + missingFields.size(), t));

    const std::string empty = "";
    CHECK(!parseTradeFast(empty.data(), empty.data(), t));
}

static void testSymbolBounds() {
    Trade t;
    const std::string longSym = std::string(64, 'A');
    t.setSymbol(longSym.data(), longSym.size());
    // Must truncate to the buffer and stay NUL-terminated.
    CHECK(std::strlen(t.symbol) == kMaxSymbolLen);
}

int main() {
    testDecimals();
    testInts();
    testPayloads();
    testSymbolBounds();

    if (failures == 0) {
        std::printf("all parser tests passed\n");
        return 0;
    }
    std::printf("%d check(s) failed\n", failures);
    return 1;
}
