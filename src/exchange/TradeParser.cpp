#include "exchange/TradeParser.h"
#include "core/FastParse.h"

#include <nlohmann/json.hpp>

/*
    Binance <symbol>@trade payload:
    {"e":"trade","E":169...,"s":"BTCUSDT","t":123,"p":"63450.12000000",
     "q":"0.00100000","T":169...,"m":true,"M":true}

    Every key is a single character and no string value in this schema can
    contain a quote or a colon, so scanning for the literal pattern
    '"' <key> '"' ':' is unambiguous. That assumption is what buys the speed;
    it is also why parseTradeJson stays around to check us in the tests.
*/

namespace {

// Returns a pointer to the first byte of the value for `key`, or nullptr.
const char* findValue(const char* b, const char* e, char key) {
    if (e - b < 4) return nullptr;
    for (const char* p = b; p <= e - 4; ++p) {
        if (p[0] == '"' && p[1] == key && p[2] == '"' && p[3] == ':') return p + 4;
    }
    return nullptr;
}

// Value is a quoted string: returns [start, stop) of the contents.
bool readString(const char* v, const char* e, const char*& start, const char*& stop) {
    if (v >= e || *v != '"') return false;
    start = v + 1;
    for (const char* p = start; p < e; ++p) {
        if (*p == '"') { stop = p; return true; }
    }
    return false;
}

// Value is a bare token (number / true / false): returns [start, stop).
bool readToken(const char* v, const char* e, const char*& start, const char*& stop) {
    if (v >= e) return false;
    start = v;
    for (const char* p = v; p < e; ++p) {
        if (*p == ',' || *p == '}' || *p == ' ') { stop = p; return true; }
    }
    stop = e;
    return true;
}

} // namespace

bool parseTradeFast(const char* begin, const char* end, Trade& out) {
    const char* s;
    const char* t;

    const char* v = findValue(begin, end, 's');
    if (!v || !readString(v, end, s, t)) return false;
    out.setSymbol(s, static_cast<std::size_t>(t - s));

    v = findValue(begin, end, 'p');
    if (!v || !readString(v, end, s, t)) return false;
    if (!fastparse::parseDecimal(s, t, out.price)) return false;

    v = findValue(begin, end, 'q');
    if (!v || !readString(v, end, s, t)) return false;
    if (!fastparse::parseDecimal(s, t, out.quantity)) return false;

    v = findValue(begin, end, 'T');
    if (!v || !readToken(v, end, s, t)) return false;
    if (!fastparse::parseInt(s, t, out.timestamp)) return false;

    v = findValue(begin, end, 'm');
    if (!v || !readToken(v, end, s, t)) return false;
    if (t - s == 4 && std::memcmp(s, "true", 4) == 0)       out.is_sell = true;
    else if (t - s == 5 && std::memcmp(s, "false", 5) == 0) out.is_sell = false;
    else return false;

    return true;
}

bool parseTradeJson(const char* begin, const char* end, Trade& out) {
    try {
        auto j = nlohmann::json::parse(begin, end);
        if (!j.contains("s") || !j.contains("p") || !j.contains("q") ||
            !j.contains("T") || !j.contains("m")) return false;

        const std::string& sym = j["s"].get_ref<const std::string&>();
        out.setSymbol(sym.data(), sym.size());
        out.price     = std::stod(j["p"].get_ref<const std::string&>());
        out.quantity  = std::stod(j["q"].get_ref<const std::string&>());
        out.timestamp = j["T"].get<long long>();
        out.is_sell   = j["m"].get<bool>();
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool parseTradeLegacy(const char* begin, const char* end, Trade& out) {
    try {
        std::string copy(begin, end);                 // beast::buffers_to_string
        auto j = nlohmann::json::parse(copy);
        std::string sym = j["s"];                     // string copy
        out.setSymbol(sym.data(), sym.size());
        out.price     = std::stod(j["p"].get<std::string>());   // copy + locale parse
        out.quantity  = std::stod(j["q"].get<std::string>());
        out.timestamp = j["T"];
        out.is_sell   = j["m"];
        return true;
    } catch (const std::exception&) {
        return false;
    }
}
