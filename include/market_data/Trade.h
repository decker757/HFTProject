#pragma once
#include <string>

struct Trade {
    std::string symbol;     //s
    double price;           //p
    double quantity;        //q
    long long timestamp;    //T
    bool is_sell;           //m (true = market sell)
};