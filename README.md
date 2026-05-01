# HFT Trading System

A C++ cryptocurrency trading system targeting Binance. Connects to the Binance WebSocket stream, runs a moving average crossover strategy, and executes orders on the Binance testnet.

## Dependencies

- Boost (Beast, Asio, Lockfree)
- OpenSSL 3
- nlohmann_json
- cmake >= 3.20

Install on macOS:

```bash
brew install boost openssl@3 nlohmann-json
```

## Build

```bash
cmake -S . -B build
cmake --build build
```

## Run

```bash
./build/apps/trader/trader
```

Set your Binance testnet API credentials in a `.env` file:

```
BINANCE_API_KEY=your_api_key
BINANCE_SECRET=your_secret_key
```
