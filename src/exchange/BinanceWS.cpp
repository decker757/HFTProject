#include <nlohmann/json.hpp>
#include <string.h>
#include "exchange/BinanceWS.h"
#include "market_data/Trade.h"
#include "core/Logging.h"

BinanceWS::BinanceWS(TradeQueue& queue, const std::string& symbol) : queue_(queue), symbol_(symbol), ssl_ctx_(net::ssl::context::tlsv12_client), ws_(net::make_strand(ioc_), ssl_ctx_) {

}

void BinanceWS::connect(const std::string& host, const std::string& port) {
    std::string stream = "/ws/" + symbol_ + "@trade";

    tcp::resolver resolver(ioc_);
    auto endpoints = resolver.resolve(host, port);

    beast::get_lowest_layer(ws_).connect(endpoints);
    ws_.next_layer().handshake(net::ssl::stream_base::client);

    ws_.handshake(host, stream);

    while (true) {
        beast::flat_buffer buffer;
        ws_.read(buffer);
        
        auto j = nlohmann::json::parse(beast::buffers_to_string(buffer.data()));
        Trade t;
        t.symbol = j["s"];
        t.price = std::stod(j["p"].get<std::string>());
        t.quantity = std::stod(j["q"].get<std::string>());
        t.timestamp = j["T"];
        t.is_sell = j["m"];

        queue_.push(t);

        std::lock_guard<std::mutex> lock(coutMtx);
        std::cout << t.symbol << " " << t.price << " " << t.quantity << "\n";
    }

}
