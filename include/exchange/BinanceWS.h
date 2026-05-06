#pragma once
#include <iostream>
#include <boost/beast.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <string.h>
#include "core/SPSCQueue.h"

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

public:
    BinanceWS(TradeQueue& queue, const std::string& symbol);
    void connect(const std::string& host, const std::string& port);
};