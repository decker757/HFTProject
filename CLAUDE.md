# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Learning Policy

This project is a personal learning exercise. When the user asks questions about the code, concepts, or implementation:

- **Never provide direct answers or working code solutions.** Instead, give guiding questions, relevant concepts to look up, and hints about what direction to think in.
- If the user is stuck, point them toward specific documentation, man pages, or topics to research — do not resolve it for them.
- If they truly cannot figure it out after guidance, tell them explicitly to research the topic further (e.g. cppreference, Boost docs, Beej's networking guide) before returning to it.
- The goal is for the user to arrive at the answer themselves. Preserve that struggle — it is the learning.

## Build

Requires macOS with Homebrew-installed dependencies: Boost, OpenSSL 3, nlohmann_json, postgresql@18.

```bash
cmake -S . -B build
cmake --build build
```

Executables are placed in `build/{trader,recorder,replayer}`.

Run individual targets:
```bash
cmake --build build --target trader
cmake --build build --target recorder
cmake --build build --target replayer
```

## Architecture

This is a cryptocurrency trading system framework targeting Binance. Three applications share a common `libcore` static library.

**Data flow (trader):** `BinanceWS` connects to Binance WebSocket stream → parses JSON trade messages → pushes `Trade` structs into `SPSCQueue` → consumer thread reads from queue → `MACrossStrategy::update()` returns `Signal` → `ExecutionEngine::executeOrder()` posts order to Binance testnet.

**Data flow (recorder):** `BinanceWS` → `SPSCQueue` → consumer thread → `MarketDataStore::updateDB()` → Postgres `trades` table.

**Key types:**
- `Trade` (`include/market_data/Trade.h`) — symbol, price, quantity, timestamp (ms), is_sell
- `TradeQueue` (`include/core/SPSCQueue.h`) — alias for `boost::lockfree::spsc_queue<Trade, boost::lockfree::capacity<1024>>`
- `BinanceWS` (`include/exchange/BinanceWS.h`) — Boost.Beast WebSocket over SSL; takes a `TradeQueue&` in its constructor; `connect()` blocks in an infinite read loop

**Applications:**
- `apps/trader/main.cpp` — multi-symbol trader (in progress); `BinanceWS` now takes a `symbol` param; 4 symbols (BTCUSDT, ETHUSDT, SOLUSDT, XRPUSDT) each get their own `BinanceWS`, `TradeQueue`, and WS thread; consumer threads not yet wired up per-symbol
- `apps/recorder/main.cpp` — live BTCUSDT stream; persists trades to Postgres via `MarketDataStore` (done)
- `apps/replayer/main.cpp` — stub for replaying historical data from Postgres (not yet implemented)

**Strategy engine (done):**
- `include/core/Signal.h` — `enum class Signal { BUY, SELL, HOLD }`
- `include/strategy/StrategyBase.h` — abstract base class with pure virtual `Signal update(Trade)` and virtual destructor
- `include/strategy/MACrossStrategy.h` — concrete subclass; owns `boost::circular_buffer<Trade> cb` and `bool prev_sma_above_lma`
- `src/strategy/MACrossStrategy.cpp` — fully implemented; `SMA_DAYS = 5`, `LMA_DAYS = 20`; computes crossover and returns `Signal`

**Market data / persistence (done):**
- `include/market_data/MarketDataStore.h` / `src/market_data/MarketDataStore.cpp` — wraps libpq; constructor connects to Postgres and creates `trades` table (`IF NOT EXISTS`); `updateDB(Trade)` inserts a row
- Postgres database: `market_data`, user: `market_user`, table: `trades (id, symbol, price, quantity, is_sell, timestamp)`

**Execution engine (done):**
- `include/execution/ExecutionEngine.h` / `src/execution/ExecutionEngine.cpp` — maintains persistent HTTPS connection to `testnet.binance.vision`; `executeOrder(Signal)` posts a signed market order (HMAC-SHA256); reads API key and secret from `.env`

**Risk module (not yet implemented):**
- `include/risk/`, `src/risk/` — intended to sit between strategy and execution; evaluates `Signal` against current portfolio before passing to `ExecutionEngine`
- Needs a `Portfolio` struct (current holdings, cash balance, max drawdown / risk limits)
- Signal flow once implemented: `MACrossStrategy` → `RiskManager` → `ExecutionEngine`

**Replayer (not yet implemented):**
- `apps/replayer/main.cpp` — reads historical `Trade` rows from Postgres and feeds them through a strategy for backtesting
- Needs a read interface on `MarketDataStore` (or a separate `MarketDataReader`)

## Roadmap

**Phase 1 (complete):** Core trading system — live multi-symbol WebSocket streaming, strategy evaluation, order execution on Binance testnet.

**Phase 2 (next):**
1. Kill switch (graceful shutdown signal handling in trader/recorder)
2. Risk module (`Portfolio` struct, `RiskManager` class)
3. Replayer / backtesting infrastructure
4. Unit tests (`tests/` directory — empty)
5. Frontend dashboard

**Phase 3:**
- ML-driven strategy improvement using backtested data

## Dependencies

| Library | Purpose |
|---------|---------|
| Boost.Beast + Boost.Asio | Async WebSocket/TCP/SSL networking |
| Boost.Lockfree | `spsc_queue` for inter-thread trade passing |
| OpenSSL 3 | TLS handshake with Binance |
| nlohmann_json | JSON parsing of Binance trade messages |
| libpq (postgresql@18) | Postgres client for `MarketDataStore` |
| dotenv-cpp | Load API keys from `.env` file |

C++20 required (`cmake_minimum_required VERSION 3.20`).
