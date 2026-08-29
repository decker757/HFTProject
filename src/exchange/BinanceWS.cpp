#include <chrono>

#include "exchange/BinanceWS.h"
#include "exchange/TradeParser.h"
#include "market_data/Trade.h"
#include "core/Logging.h"

namespace {
inline uint64_t nowNs() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
}
}

BinanceWS::BinanceWS(TradeQueue& queue, const std::string& symbol, ParseMode mode)
    : queue_(queue),
      symbol_(symbol),
      ssl_ctx_(net::ssl::context::tlsv12_client),
      ws_(net::make_strand(ioc_), ssl_ctx_),
      mode_(mode) {
}

void BinanceWS::connect(const std::string& host, const std::string& port) {
    std::string stream = "/ws/" + symbol_ + "@trade";

    tcp::resolver resolver(ioc_);
    auto endpoints = resolver.resolve(host, port);

    beast::get_lowest_layer(ws_).connect(endpoints);
    ws_.next_layer().handshake(net::ssl::stream_base::client);

    ws_.handshake(host, stream);

    // Hoisted out of the loop. Constructing a flat_buffer per message meant a
    // malloc and free on every trade; consume() rewinds it for reuse instead.
    beast::flat_buffer buffer;

    while (running_.load(std::memory_order_relaxed)) {
        buffer.consume(buffer.size());
        ws_.read(buffer);

        // Stamp as close to the wire as we can get: everything measured
        // downstream is our own cost, not the network's.
        const uint64_t recv_ns = nowNs();

        // Parse straight out of the read buffer. buffers_to_string used to copy
        // the whole payload into a fresh std::string before nlohmann even
        // started.
        auto data = buffer.data();
        const char* begin = static_cast<const char*>(data.data());
        const char* end   = begin + data.size();

        Trade t;
        if (!parseTrade(begin, end, t, mode_)) {
            parseErrors_.fetch_add(1, std::memory_order_relaxed);
            continue;
        }
        t.recv_ns = recv_ns;

        received_.fetch_add(1, std::memory_order_relaxed);

        // push() returns false when the ring is full. Ignoring it silently
        // discarded trades whenever the consumer fell behind.
        if (!queue_.push(t)) {
            dropped_.fetch_add(1, std::memory_order_relaxed);
        }
    }
}
