#pragma once

#include <string.h>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <unordered_map>
#include <mutex>

#include "core/Signal.h"

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = net::ssl;
using tcp = net::ip::tcp;

class ExecutionEngine {
private:
    net::io_context ioc;
    ssl::context ctx {ssl::context::tlsv12_client};
    tcp::resolver resolver;
    beast::ssl_stream<beast::tcp_stream> stream;

    std::string API_KEY;
    std::string SECRET_KEY;
    std::string HOST_URL;
    std::unordered_map<std::string, long long> lastOrderTime_;
    
    std::unordered_map<std::string, std::string> minQty = {
        {"BTCUSDT", "0.001"},
        {"ETHUSDT", "0.01"},
        {"SOLUSDT", "0.1"},
        {"XRPUSDT", "1"}
    };

    std::mutex mtx_;
    
public:
    ExecutionEngine();
    void executeOrder(Signal, std::string currency);
};