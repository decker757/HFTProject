# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Learning Policy

This project is a personal learning exercise. When the user asks questions about the code, concepts, or implementation:

- **Never provide direct answers or working code solutions.** Instead, give guiding questions, relevant concepts to look up, and hints about what direction to think in.
- If the user is stuck, point them toward specific documentation, man pages, or topics to research — do not resolve it for them.
- If they truly cannot figure it out after guidance, tell them explicitly to research the topic further (e.g. cppreference, Boost docs, Beej's networking guide) before returning to it.
- The goal is for the user to arrive at the answer themselves. Preserve that struggle — it is the learning.

## Build

Requires macOS with Homebrew-installed dependencies: Boost, OpenSSL 3, nlohmann_json.

```bash
cmake -S . -B build
cmake --build build
```

Executables are placed in `build/apps/{trader,recorder,replayer}/`.

Run individual targets:
```bash
cmake --build build --target trader
cmake --build build --target recorder
cmake --build build --target replayer
```

## Architecture

This is a cryptocurrency trading system framework targeting Binance. Three applications share a common `libcore` shared library built from `src/exchange/BinanceWS.cpp`.

**Data flow:** `BinanceWS` connects to Binance WebSocket stream → parses JSON trade messages → pushes `Trade` structs into `SPSCQueue` (Boost lock-free, capacity 1024) → consumer thread reads from queue.

**Key types:**
- `Trade` (`include/market_data/Trade.h`) — symbol, price, quantity, timestamp (ms), is_sell
- `TradeQueue` (`include/core/SPSCQueue.h`) — alias for `boost::lockfree::spsc_queue<Trade, boost::lockfree::capacity<1024>>`
- `BinanceWS` (`include/exchange/BinanceWS.h`) — Boost.Beast WebSocket over SSL; takes a `TradeQueue&` in its constructor; `connect()` blocks in an infinite read loop

**Applications:**
- `apps/trader/` — live BTCUSDT stream from `stream.binance.com:443/ws/btcusdt@trade` (implemented)
- `apps/recorder/` — stub for persisting trade data
- `apps/replayer/` — stub for replaying historical data

**Strategy engine (in progress):**
- `include/core/Signal.h` — `enum class Signal { Buy, Sell, Hold }` (done)
- `include/strategy/StrategyBase.h` — abstract base class with pure virtual `Signal update(Trade)` and virtual destructor (done)
- `include/strategy/MACrossStrategy.h` — concrete subclass; owns `boost::circular_buffer<Trade> cb` and `bool prev_sma_above_lma` (done)
- `src/strategy/MACrossStrategy.cpp` — constructor initializes `cb(capacity)` and `prev_sma_above_lma(false)`; `update(Trade t)` body in progress

**Strategy engine design decisions:**
- `main` owns all SPSC queues, passes references to both the aggregator thread and each strategy
- Aggregator thread fans out incoming trades from `BinanceWS` to per-strategy SPSC queues (one queue per strategy)
- Each strategy maintains its own `boost::circular_buffer` sized to the LMA period
- MA crossover detection uses a single `bool prev_sma_above_lma` — flips when SMA/LMA relationship changes between ticks
- `Signal` returned from `update()` — `Hold` when buffer not yet full (warm-up period), `Buy`/`Sell` on crossover
- Constants `SMA_DAYS = 5`, `LMA_DAYS = 20` defined at file scope in `MACrossStrategy.cpp`

**Next steps for `MACrossStrategy::update()`:**
1. Push incoming trade price into `cb`
2. Guard with `cb.size() >= LMA_DAYS` before computing
3. Compute SMA (average of last `SMA_DAYS` prices) and LMA (average of all `LMA_DAYS` prices)
4. Compare current SMA vs LMA relationship to `prev_sma_above_lma` to detect crossover
5. Update `prev_sma_above_lma` and return appropriate `Signal`

**Skeleton modules (not yet implemented):** `include/execution/`, `include/risk/`, `src/execution/`, `src/risk/`, `src/market_data/`, `tests/`

## Dependencies

| Library | Purpose |
|---------|---------|
| Boost.Beast + Boost.Asio | Async WebSocket/TCP/SSL networking |
| Boost.Lockfree | `spsc_queue` for inter-thread trade passing |
| OpenSSL 3 | TLS handshake with Binance |
| nlohmann_json | JSON parsing of Binance trade messages |

C++20 required (`cmake_minimum_required VERSION 3.20`).
