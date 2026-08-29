#include "exchange/BinanceWS.h"
#include "execution/ExecutionEngine.h"
#include "execution/OrderRouter.h"
#include "strategy/MACrossStrategy.h"
#include "strategy/BollingerBandsStrategy.h"
#include "strategy/MomentumStrategy.h"
#include "strategy/RSIStrategy.h"
#include "strategy/StrategyEvaluator.h"
#include "core/Backoff.h"
#include "core/LatencyHistogram.h"
#include "core/Logging.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

std::atomic<bool> running{true};

namespace {

inline uint64_t nowNs() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
}

constexpr std::array<const char*, 4> kSymbols = {"btcusdt", "ethusdt", "solusdt", "xrpusdt"};

// One of these per symbol. Each owns its own queue, strategies, and histogram
// so nothing on the market-data path is shared between threads.
struct SymbolPipeline {
    explicit SymbolPipeline(const char* sym)
        : lower(sym), upper(sym), ws(queue, lower) {
        std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    }

    std::string lower;
    std::string upper;
    TradeQueue queue;
    BinanceWS ws;
    LatencyHistogram recvToSignal{1u << 20};
};

void signalHandler(int) {
    running.store(false, std::memory_order_relaxed);
}

} // namespace

int main() {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::vector<std::unique_ptr<SymbolPipeline>> pipes;
    pipes.reserve(kSymbols.size());
    for (const char* s : kSymbols) pipes.push_back(std::make_unique<SymbolPipeline>(s));

    ExecutionEngine exec;

    // The sender thread owns the ExecutionEngine. Strategy threads never touch
    // the network; they hand a signal to the router and go back to reading.
    OrderRouter router(exec);
    router.start();

    std::vector<std::thread> wsThreads;
    std::vector<std::thread> consumers;

    for (auto& p : pipes) {
        wsThreads.emplace_back([&p] {
            try {
                p->ws.connect("stream.binance.com", "443");
            } catch (const std::exception& e) {
                std::lock_guard<std::mutex> lock(coutMtx);
                std::cerr << p->lower << " websocket ended: " << e.what() << "\n";
            }
        });

        consumers.emplace_back([&p, &router] {
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

            while (running.load(std::memory_order_relaxed)) {
                if (!p->queue.pop(t)) { backoff.pause(); continue; }
                backoff.reset();

                StrategyBase* strat = eval.select(t);
                Signal sig = strat->update(t);

                // Everything from the websocket read to here is our own cost.
                // The order round trip is deliberately outside this measurement
                // and is reported separately by the router.
                p->recvToSignal.record(nowNs() - t.recv_ns);

                if (sig != Signal::HOLD) {
                    router.submit(sig, p->upper.c_str(), nowNs());
                }
            }
        });
    }

    std::thread balanceThread([&exec] {
        const std::unordered_set<std::string> watchlist = {"USDT", "BTC", "ETH", "SOL", "XRP"};
        while (running.load(std::memory_order_relaxed)) {
            try {
                auto balances = exec.getBalance();
                std::lock_guard<std::mutex> lock(coutMtx);
                for (auto& [asset, free] : balances) {
                    if (watchlist.count(asset)) std::cout << asset << ": " << free << "\n";
                }
            } catch (const std::exception& e) {
                std::lock_guard<std::mutex> lock(coutMtx);
                std::cerr << "balance poll failed: " << e.what() << "\n";
            }
            // Sliced so Ctrl-C is not stuck behind a 30 second sleep.
            for (int i = 0; i < 300 && running.load(std::memory_order_relaxed); ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    });

    for (auto& c : consumers) c.join();
    balanceThread.join();
    for (auto& p : pipes) p->ws.stop();
    router.stop();

    LatencyHistogram all{1u << 22};
    std::cout << "\n=== shutdown ===\n";
    for (auto& p : pipes) {
        std::cout << p->upper
                  << "  received " << p->ws.received()
                  << "  dropped "  << p->ws.dropped()
                  << "  parse_errors " << p->ws.parseErrors()
                  << "  signals " << p->recvToSignal.count() << "\n";
        all.merge(p->recvToSignal);
    }
    std::cout << "\norders  submitted " << router.submitted()
              << "  sent " << router.sent()
              << "  dropped(queue full) " << router.dropped() << "\n\n";

    std::cout << all.report("receipt -> signal (all symbols)");
    std::cout << router.queueWait().report("\norder queue wait");
    std::cout << router.sendTime().report("\norder round trip (network)");

    // The websocket reads are parked in a blocking recv and only return when
    // the next trade arrives. Rather than join threads that may sit for a
    // while on a quiet stream, exit once the measurements are out.
    // Proper fix is async Beast reads with a cancellation slot.
    std::cout.flush();
    std::_Exit(0);
}
