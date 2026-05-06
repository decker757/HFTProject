#include "market_data/Trade.h"
#include "market_data/MarketDataStore.h"
#include "exchange/BinanceWS.h"

#include <thread>
#include <atomic>

std::atomic<bool> running{true};

int main() {
    TradeQueue queueBTC;
    BinanceWS wsBTC(queueBTC, "btcusdt");
    MarketDataStore dataStore("127.0.0.1", 5432, "market_data", "market_user", "root");

    std::thread wsBTCThread([&] () {
        wsBTC.connect("stream.binance.com", "443");
    });

    std::thread updateThread([&] () {
        while (running.load(std::memory_order_relaxed)) {
            Trade latestTrade;
            if (queueBTC.pop(latestTrade)) {
                dataStore.updateDB(latestTrade);
            } else {
                std::this_thread::yield();
            }
        }
    });

    if (wsBTCThread.joinable()) wsBTCThread.join();
    if (updateThread.joinable()) updateThread.join();

    return 0;
}
