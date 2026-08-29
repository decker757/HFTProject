#pragma once
#include <cstdint>
#include <cstring>

/*
    Trivially copyable so it can sit in a lock-free ring buffer with no
    allocation and no destructor work on push/pop.

    symbol was a std::string. Short Binance symbols fit in SSO, so that was
    never a malloc, but it made Trade non-trivial and 32 bytes wider than it
    needed to be. A fixed array keeps the queue element flat.
*/
inline constexpr std::size_t kMaxSymbolLen = 12;   // longest Binance symbol today is 12 chars

struct Trade {
    char      symbol[kMaxSymbolLen + 1] = {};   // NUL-terminated
    double    price = 0.0;                      //p
    double    quantity = 0.0;                   //q
    long long timestamp = 0;                    //T (exchange time, ms)
    uint64_t  recv_ns = 0;                      // local steady_clock ns, stamped at WS read
    bool      is_sell = false;                  //m (true = market sell)

    void setSymbol(const char* s, std::size_t n) {
        if (n > kMaxSymbolLen) n = kMaxSymbolLen;
        std::memcpy(symbol, s, n);
        symbol[n] = '\0';
    }
};
