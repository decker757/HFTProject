#include "exchange/BinanceWS.h"
#include "strategy/MACrossStrategy.h"
#include "execution/ExecutionEngine.h"

#include <thread>
#include <atomic>

std::atomic<bool> running{true};


int main() {
    TradeQueue queue;
    BinanceWS ws(queue);
    MACrossStrategy strat;
    ExecutionEngine exec;
    

    std::thread wsThread([&] () {
        ws.connect("stream.binance.com", "443", "/ws/btcusdt@trade");
    });

    std::thread updateThread([&] () {
        while (running.load(std::memory_order_relaxed)) {
            Trade latestTrade;
            if (queue.pop(latestTrade)) {
                Signal sig = strat.update(latestTrade);
                exec.executeOrder(sig);
            } else {
                std::this_thread::yield();
            }
        }
    });

    if (wsThread.joinable()) wsThread.join();
    if (updateThread.joinable()) updateThread.join();

    return 0;
};