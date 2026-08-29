#include "core/Backoff.h"
#include "core/LatencyHistogram.h"
#include "core/SPSCQueue.h"
#include "exchange/TradeParser.h"
#include "market_data/Trade.h"
#include "strategy/BollingerBandsStrategy.h"
#include "strategy/MACrossStrategy.h"
#include "strategy/MomentumStrategy.h"
#include "strategy/RSIStrategy.h"
#include "strategy/StrategyEvaluator.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

/*
    Deterministic benchmark. No network, no credentials, no Binance account.
    Replays a captured @trade payload through the same code the live trader
    runs so the numbers are reproducible on any machine.
*/

namespace {

inline uint64_t nowNs() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
}

// A real Binance BTCUSDT@trade frame.
const std::string kPayload =
    R"({"e":"trade","E":1746000000123,"s":"BTCUSDT","t":4738291,"p":"63450.12000000",)"
    R"("q":"0.00134000","T":1746000000121,"m":true,"M":true})";

constexpr int kParseIters = 200000;
constexpr int kPipeIters  = 200000;

double clockOverheadNs() {
    constexpr int N = 100000;
    uint64_t start = nowNs();
    uint64_t sink = 0;
    for (int i = 0; i < N; ++i) sink += nowNs();
    uint64_t end = nowNs();
    if (sink == 0) std::cout << "";   // keep the loop
    return static_cast<double>(end - start) / N;
}

using ParseFn = bool (*)(const char*, const char*, Trade&);

void benchParse(const char* label, ParseFn fn, double overhead) {
    const char* b = kPayload.data();
    const char* e = b + kPayload.size();

    Trade t;
    // Warm up caches and branch predictors.
    for (int i = 0; i < 10000; ++i) fn(b, e, t);

    LatencyHistogram h{kParseIters};
    double sink = 0;

    uint64_t batchStart = nowNs();
    for (int i = 0; i < kParseIters; ++i) {
        uint64_t s = nowNs();
        fn(b, e, t);
        h.record(nowNs() - s);
        sink += t.price;
    }
    uint64_t batchEnd = nowNs();

    if (sink < 0) std::cout << "";   // keep the loop

    const double perOp = static_cast<double>(batchEnd - batchStart) / kParseIters;
    std::printf("%s\n", label);
    std::printf("  batch mean   %8.1f ns/op  (%.1f ns after removing %.1f ns clock overhead)\n",
                perOp, perOp - overhead, overhead);
    std::printf("  p50 %6llu ns   p99 %6llu ns   max %8llu ns\n\n",
                (unsigned long long)h.percentile(50.0),
                (unsigned long long)h.percentile(99.0),
                (unsigned long long)h.max());
}

void benchPipeline() {
    TradeQueue queue;
    std::atomic<bool> done{false};
    LatencyHistogram h{kPipeIters};

    std::thread consumer([&] {
        MACrossStrategy ma;
        BollingerBandsStrategy bb;
        MomentumStrategy mom;
        RSIStrategy rsi;
        StrategyEvaluator eval({
            {StrategyType::MA_CROSS,  &ma},
            {StrategyType::BOLLINGER, &bb},
            {StrategyType::MOMENTUM,  &mom},
            {StrategyType::RSI,       &rsi}
        });

        Backoff backoff;
        Trade t;
        int seen = 0;
        while (seen < kPipeIters) {
            if (!queue.pop(t)) {
                if (done.load(std::memory_order_relaxed) && !queue.read_available()) break;
                backoff.pause();
                continue;
            }
            backoff.reset();
            StrategyBase* strat = eval.select(t);
            strat->update(t);
            h.record(nowNs() - t.recv_ns);
            ++seen;
        }
    });

    const char* b = kPayload.data();
    const char* e = b + kPayload.size();
    uint64_t dropped = 0;

    for (int i = 0; i < kPipeIters; ++i) {
        Trade t;
        parseTradeFast(b, e, t);
        // Vary the price so the strategies actually do work.
        t.price += (i % 97) * 0.5;
        t.recv_ns = nowNs();
        while (!queue.push(t)) {
            ++dropped;
            std::this_thread::yield();
        }
    }
    done.store(true, std::memory_order_relaxed);
    consumer.join();

    std::cout << h.report("parse -> queue -> strategy signal (in-process)");
    std::cout << "  producer stalls on full ring: " << dropped << "\n\n";
}

} // namespace

int main() {
    // Correctness first: the fast parser must agree with the reference parser.
    Trade fast, ref;
    const char* b = kPayload.data();
    const char* e = b + kPayload.size();
    if (!parseTradeFast(b, e, fast) || !parseTradeJson(b, e, ref)) {
        std::cerr << "parser failed on sample payload\n";
        return 1;
    }
    if (std::strcmp(fast.symbol, ref.symbol) != 0 || fast.price != ref.price ||
        fast.quantity != ref.quantity || fast.timestamp != ref.timestamp ||
        fast.is_sell != ref.is_sell) {
        std::cerr << "fast parser disagrees with reference parser\n";
        return 1;
    }

    const double overhead = clockOverheadNs();
    std::printf("payload %zu bytes, %d iterations per parser\n\n", kPayload.size(), kParseIters);

    benchParse("original hot path (string copy + DOM parse + stod)", parseTradeLegacy, overhead);
    benchParse("DOM parse, no redundant copies",                 parseTradeJson,   overhead);
    benchParse("hand-rolled scan (current hot path)",             parseTradeFast,   overhead);
    benchPipeline();
    return 0;
}
