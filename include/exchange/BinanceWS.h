#pragma once
#include <atomic>
#include <cstdint>
#include <iostream>
#include <boost/beast.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <string.h>

#include "core/SPSCQueue.h"
#include "exchange/TradeParser.h"

namespace net = boost::asio;
namespace beast = boost::beast;
namespace websocket = boost::beast::websocket;
using tcp = net::ip::tcp;

class BinanceWS {
private:
    TradeQueue& queue_;
    std::string symbol_;
    net::io_context ioc_;
    net::ssl::context ssl_ctx_;
    websocket::stream<net::ssl::stream<beast::tcp_stream>> ws_;
    ParseMode mode_;

    std::atomic<bool>     running_{true};
    std::atomic<uint64_t> received_{0};
    std::atomic<uint64_t> dropped_{0};      // queue was full: trade lost
    std::atomic<uint64_t> parseErrors_{0};

public:
    BinanceWS(TradeQueue& queue, const std::string& symbol, ParseMode mode = ParseMode::Fast);
    void connect(const std::string& host, const std::string& port);

    // Checked once per message, so the loop exits after the next trade arrives.
    void stop() { running_.store(false, std::memory_order_relaxed); }

    uint64_t received()    const { return received_.load(std::memory_order_relaxed); }
    uint64_t dropped()     const { return dropped_.load(std::memory_order_relaxed); }
    uint64_t parseErrors() const { return parseErrors_.load(std::memory_order_relaxed); }
    const std::string& symbol() const { return symbol_; }
};
