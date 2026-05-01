#include "exchange/BinanceWS.h"
#include "strategy/MACrossStrategy.h"
#include "execution/ExecutionEngine.h"

int main() {
    TradeQueue queue;
    BinanceWS ws(queue);
    ws.connect("stream.binance.com", "443", "/ws/btcusdt@trade");
    



};