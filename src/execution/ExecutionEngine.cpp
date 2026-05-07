#include <string.h>
#include <iomanip>
#include <iostream>
#include <thread>
#include <openssl/hmac.h>
#include <chrono>
#include <laserpants/dotenv/dotenv.h>
#include <boost/beast/version.hpp>
#include <nlohmann/json.hpp>

#include "execution/ExecutionEngine.h"
#include "core/Signal.h"
#include "core/HMACSignature.h"
#include "core/Logging.h"

/*
    Execution Engine:
    1. Receive trade signal from the strategy engine(s)
    2. Execute it on binance test-net
*/

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = net::ssl;
using tcp = net::ip::tcp;

ExecutionEngine::ExecutionEngine() : resolver(ioc), stream(ioc, ctx), accountStream(accountIoc, accountCtx) {
    dotenv::init();
    API_KEY = dotenv::getenv("BINANCE_API_KEY", "skibidi");
    SECRET_KEY = dotenv::getenv("BINANCE_SECRET", "skibidi");
    HOST_URL = "testnet.binance.vision";

    boost::system::error_code ec;
    ctx.set_default_verify_paths(ec);
    ctx.set_verify_mode(ssl::verify_peer);
    accountCtx.set_default_verify_paths(ec);
    accountCtx.set_verify_mode(ssl::verify_peer);

    try {
        auto const results = resolver.resolve(HOST_URL, "443");
        beast::get_lowest_layer(stream).connect(results);
        SSL_set_tlsext_host_name(stream.native_handle(), HOST_URL.c_str());
        stream.handshake(ssl::stream_base::client);

        tcp::resolver accountResolver(accountIoc);
        auto const accountResults = accountResolver.resolve(HOST_URL, "443");
        beast::get_lowest_layer(accountStream).connect(accountResults);
        SSL_set_tlsext_host_name(accountStream.native_handle(), HOST_URL.c_str());
        accountStream.handshake(ssl::stream_base::client);
    } catch (std::exception const& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

void ExecutionEngine::executeOrder(Signal s, std::string currency) {
    std::lock_guard<std::mutex> lock(mtx_);
    std::string sig = "";

    if (s == Signal::BUY) sig = "BUY";
    else if (s == Signal::SELL) sig = "SELL";
    else sig = "HOLD";

    try { 
        //Fresh timestamp
        if (sig != "HOLD") {
            long long now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            if (now - lastOrderTime_[currency] < 1000) return;
            lastOrderTime_[currency] = now;

            std::string quantity = minQty.count(currency) ? minQty[currency] : "0.001";
            std::string query = "symbol=" + currency + "&side=" + sig + "&type=MARKET&quantity=" + quantity + "&timestamp=" + std::to_string(now);
            std::string signature = hmac_sha256(SECRET_KEY, query);
            std::string target = "/api/v3/order?" + query + "&signature=" + signature;

            http::request<http::empty_body> req{http::verb::post, target, 11};
            req.set(http::field::host, HOST_URL);
            req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
            req.set("X-MBX-APIKEY", API_KEY);
            req.set(http::field::connection, "keep-alive");
            req.keep_alive(true);

            http::write(stream, req);
            beast::flat_buffer buffer;
            http::response<http::string_body> res;
            http::read(stream, buffer, res);

            std::lock_guard<std::mutex> lock(coutMtx);
            std::cout << res.body() << std::endl;
        }
    } catch (std::exception const& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        reconnect();
    }
}

std::unordered_map<std::string, double> ExecutionEngine::getBalance() {
    std::lock_guard<std::mutex> lock(accountMtx_);

    long long ts = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    std::string query = "timestamp=" + std::to_string(ts);
    std::string signature = hmac_sha256(SECRET_KEY, query);
    std::string target = "/api/v3/account?" + query + "&signature=" + signature;

    http::request<http::empty_body> req{http::verb::get, target, 11};
    req.set(http::field::host, HOST_URL);
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    req.set("X-MBX-APIKEY", API_KEY);
    req.set(http::field::connection, "keep-alive");
    req.keep_alive(true);

    http::write(accountStream, req);
    beast::flat_buffer buffer;
    http::response<http::string_body> res;
    http::read(accountStream, buffer, res);

    auto j = nlohmann::json::parse(res.body());
    std::unordered_map<std::string, double> balances;
    for (auto& b : j["balances"]) {
        double free = std::stod(b["free"].get<std::string>());
        if (free > 0.0) balances[b["asset"]] = free;
    }
    return balances;
}

void ExecutionEngine::reconnect() {
    try { stream.shutdown(); } catch (...) {}
    beast::get_lowest_layer(stream).close();

    const int maxRetries = 3;
    for (int i = 0; i < maxRetries; i++) {
        try {
            auto const results = resolver.resolve(HOST_URL, "443");
            beast::get_lowest_layer(stream).connect(results);
            SSL_set_tlsext_host_name(stream.native_handle(), HOST_URL.c_str());
            stream.handshake(ssl::stream_base::client);
            std::cerr << "Reconnect successful\n";
            return; // success
        } catch (std::exception const& e) {
            std::cerr << "Reconnect attempt " << (i+1) << " failed: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    std::cerr << "All reconnect attempts failed — stream is broken\n";
}