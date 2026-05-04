#include "market_data/Trade.h"
#include "market_data/MarketDataStore.h"
#include "exchange/BinanceWS.h"

#include <thread>
#include <atomic>

std::atomic<bool> running{true};

int main() {
    TradeQueue queue;
    BinanceWS ws(queue);
    MarketDataStore dataStore("127.0.0.1", 5432, "market_data", "market_user", "root");

    std::thread wsThread([&] () {
        ws.connect("stream.binance.com", "443", "/ws/btcusdt@trade");
    });

    std::thread updateThread([&] () {
        while (running.load(std::memory_order_relaxed)) {
            Trade latestTrade;
            if (queue.pop(latestTrade)) {
                dataStore.updateDB(latestTrade);
            } else {
                std::this_thread::yield();
            }
        }
    });

    if (wsThread.joinable()) wsThread.join();
    if (updateThread.joinable()) updateThread.join();


    return 0;
}
