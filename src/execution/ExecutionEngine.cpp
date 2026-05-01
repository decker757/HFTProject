#include <string.h>
#include <iomanip>
#include <iostream>

#include <openssl/hmac.h>
#include <chrono>
#include <laserpants/dotenv/dotenv.h>
#include <boost/beast/version.hpp>

#include "execution/ExecutionEngine.h"
#include "strategy/MACrossStrategy.h"
#include "core/Signal.h"
#include "core/HMACSignature.h"

/*
    Execution Engine:
    1. Receive trade signal from the strategy engine(s)
    2. Execute it on binance test-net
    3. 

*/

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = net::ssl;
using tcp = net::ip::tcp;

ExecutionEngine::ExecutionEngine() : resolver(ioc), stream(ioc, ctx) {
    dotenv::init();
    API_KEY = dotenv::getenv("BINANCE_API_KEY", "skibidi");
    SECRET_KEY = dotenv::getenv("BINANCE_SECRET", "skibidi");
    HOST_URL = "testnet.binance.vision";

    try {
        auto const results = resolver.resolve(HOST_URL, "443");
        beast::get_lowest_layer(stream).connect(results);
        stream.handshake(ssl::stream_base::client);

        ts = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

    } catch (std::exception const& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

void ExecutionEngine::executeOrder(Signal s) {
    std::string sig = "";
    if (s == Signal::BUY) sig = "BUY";
    else if (s == Signal::SELL) sig = "SELL";
    else sig = "HOLD";

    try { 
        std::string query = "symbol=BTCUSDT&side=" + sig +"&type=MARKET&quantity=0.001&timestamp=" + std::to_string(ts);
        std::string signature = hmac_sha256(SECRET_KEY, query);
        std::string target = "/api/v3/order?" + query + "&signature=" + signature;

        http::request<http::empty_body> req{http::verb::post, target, 11};
        req.set(http::field::host, HOST_URL);
        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
        req.set("X-MBX-APIKEY", API_KEY);

        http::write(stream, req);
        beast::flat_buffer buffer;
        http::response<http::string_body> res;
        http::read(stream, buffer, res);

        std::cout << res.body() << std::endl;
    } catch (std::exception const& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}