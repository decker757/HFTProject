# HFT Trading System

A C++ cryptocurrency trading system targeting Binance. Streams live trade data from multiple symbols via Binance WebSocket, dynamically selects trading strategies, and executes orders on the Binance testnet.

## Roadmap

- **Phase 1 (complete):** Core trading system — live multi-symbol streaming, strategy evaluation, order execution
- **Phase 2:** Kill switch, risk module, replayer/backtesting, unit tests, frontend dashboard
- **Phase 3:** ML-driven strategy improvement using backtested data

## Architecture

Three applications share a common `libcore` static library:

- **trader** — connects to live Binance WebSocket streams for BTCUSDT, ETHUSDT, SOLUSDT, XRPUSDT; each symbol runs on its own thread with an independent queue, strategy evaluator, and execution engine
- **recorder** — persists live trade data to a Postgres database
- **replayer** — replays historical trades from Postgres for backtesting (Phase 2)

**Data flow (trader):** `BinanceWS` → `SPSCQueue` → consumer thread → `StrategyEvaluator` → strategy (`MACross` / `BollingerBands` / `Momentum` / `RSI`) → `ExecutionEngine`

**Strategies:**
- `MACrossStrategy` — moving average crossover (SMA 5 / LMA 20)
- `BollingerBandsStrategy` — buy at lower band, sell at upper band
- `MomentumStrategy` — buy/sell on price change exceeding a threshold
- `RSIStrategy` — buy when oversold (<30), sell when overbought (>70); uses Wilder's smoothing

`StrategyEvaluator` selects the active strategy per trade based on coefficient of variation and price deviation from the rolling mean.

## Dependencies

- Boost (Beast, Asio, Lockfree)
- OpenSSL 3
- nlohmann_json
- postgresql@18
- cmake >= 3.20

Install on macOS:

```bash
brew install boost openssl@3 nlohmann-json postgresql@18
```

## Build

```bash
cmake -S . -B build
cmake --build build
```

Run individual targets:

```bash
cmake --build build --target trader
cmake --build build --target recorder
cmake --build build --target replayer
```

## Run

```bash
./build/apps/trader/trader
./build/apps/recorder/recorder
```

Set your Binance testnet API credentials in a `.env` file:

```
BINANCE_API_KEY=your_api_key
BINANCE_SECRET=your_secret_key
```

Postgres connection is hardcoded to database `market_data`, user `market_user`. Create them before running the recorder:

```sql
CREATE USER market_user WITH PASSWORD 'your_password';
CREATE DATABASE market_data OWNER market_user;
```
