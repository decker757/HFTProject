#include "exchange/BinanceWS.h"
#include "strategy/MACrossStrategy.h"
#include "execution/ExecutionEngine.h"

#include <thread>
#include <atomic>

std::atomic<bool> running{true};


int main() {
    TradeQueue queueBTC;
    BinanceWS wsBTC(queueBTC, "btcusdt");

    TradeQueue queueETH;
    BinanceWS wsETH(queueETH, "ethusdt");

    TradeQueue queueSOL;
    BinanceWS wsSOL(queueSOL, "solusdt");

    TradeQueue queueXRP;
    BinanceWS wsXRP(queueXRP, "xrpusdt");

    MACrossStrategy strat;
    ExecutionEngine exec;
    

    std::thread wsBTCThread([&] () {
        wsBTC.connect("stream.binance.com", "443");
    });

    std::thread wsETHThread([&] () {
        wsETH.connect("stream.binance.com", "443");
    });

    std::thread wsSOLThread([&] () {
        wsSOL.connect("stream.binance.com", "443");
    });

    std::thread wsXRPThread([&] () {
        wsXRP.connect("stream.binance.com", "443");
    });

    std::thread updateThread([&] () {
        while (running.load(std::memory_order_relaxed)) {
            Trade latestTrade;
            if (queueBTC.pop(latestTrade)) {
                Signal sig = strat.update(latestTrade);
                exec.executeOrder(sig);
            } else {
                std::this_thread::yield();
            }
        }
    });

    if (wsBTCThread.joinable()) wsBTCThread.join();
    if (wsETHThread.joinable()) wsETHThread.join();
    if (wsSOLThread.joinable()) wsSOLThread.join();
    if (wsXRPThread.joinable()) wsXRPThread.join();
    if (updateThread.joinable()) updateThread.join();

    return 0;
};