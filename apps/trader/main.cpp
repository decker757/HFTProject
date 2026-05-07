#include "exchange/BinanceWS.h"
#include "execution/ExecutionEngine.h"
#include "strategy/MACrossStrategy.h"
#include "strategy/BollingerBandsStrategy.h"
#include "strategy/MomentumStrategy.h"
#include "strategy/RSIStrategy.h"
#include "strategy/StrategyEvaluator.h"

#include <thread>
#include <atomic>

std::atomic<bool> running{true};

int main() {
    TradeQueue queueBTC, queueETH, queueSOL, queueXRP;
    BinanceWS wsBTC(queueBTC, "btcusdt");
    BinanceWS wsETH(queueETH, "ethusdt");
    BinanceWS wsSOL(queueSOL, "solusdt");
    BinanceWS wsXRP(queueXRP, "xrpusdt");

    ExecutionEngine exec;

    auto makeConsumer = [&](TradeQueue& queue, const std::string& symbol) {
        std::string upperSymbol = symbol;
        std::transform(upperSymbol.begin(), upperSymbol.end(), upperSymbol.begin(), ::toupper);

        return std::thread([&exec, q = &queue, upperSymbol]() {
            MACrossStrategy ma;
            BollingerBandsStrategy bb;
            MomentumStrategy mom;
            RSIStrategy rsi;

            StrategyEvaluator eval({
                {StrategyType::MA_CROSS, &ma},
                {StrategyType::BOLLINGER, &bb},
                {StrategyType::MOMENTUM, &mom},
                {StrategyType::RSI, &rsi}
            });

            while (running.load(std::memory_order_relaxed)) {
                Trade t;
                if (q->pop(t)) {
                    StrategyBase* strat = eval.select(t);
                    Signal sig = strat->update(t);
                    if (sig != Signal::HOLD) exec.executeOrder(sig, upperSymbol);
                } else {
                    std::this_thread::yield();
                }
            }
        });
    };

    std::thread wsBTCThread([&]() { wsBTC.connect("stream.binance.com", "443"); });
    std::thread wsETHThread([&]() { wsETH.connect("stream.binance.com", "443"); });
    std::thread wsSOLThread([&]() { wsSOL.connect("stream.binance.com", "443"); });
    std::thread wsXRPThread([&]() { wsXRP.connect("stream.binance.com", "443"); });

    std::thread updateBTCThread = makeConsumer(queueBTC, "btcusdt");
    std::thread updateETHThread = makeConsumer(queueETH, "ethusdt");
    std::thread updateSOLThread = makeConsumer(queueSOL, "solusdt");
    std::thread updateXRPThread = makeConsumer(queueXRP, "xrpusdt");

    wsBTCThread.join();
    wsETHThread.join();
    wsSOLThread.join();
    wsXRPThread.join();
    updateBTCThread.join();
    updateETHThread.join();
    updateSOLThread.join();
    updateXRPThread.join();

    return 0;
}