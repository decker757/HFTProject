#include "strategy/MACrossStrategy.h"
#include "core/Signal.h"
#include "market_data/Trade.h"

/*
    1. Trade is received from the SPSCQueue from main
    2. Create a circular buffer to store the Trade events
    3. Once circular buffer has enough data, calculate
        SMA and LMA
    4. Determine which is larger, SMA and LMA
    5. Calculate the next SMA and LMA and then do a cross and return Signal
*/

const int SMA_DAYS = 5;
const int LMA_DAYS = 20;


MACrossStrategy::MACrossStrategy() : prev_sma_above_lma(false), cb(LMA_DAYS) {
   
}

Signal MACrossStrategy::update(Trade t) {
    double SMA = 0.0;
    double LMA = 0.0;

    cb.push_back(t);

    if (cb.size() >= SMA_DAYS && cb.size() < LMA_DAYS) { // buffer does not have enough for LMA calculation.
        SMA = calculateSMA();
    }
    else if (cb.size() == LMA_DAYS) { // SMA & LMA is possible to calculate
        SMA = calculateSMA();
        LMA = calculateLMA();
    }

    if (!prev_sma_above_lma && SMA > LMA) { 
        prev_sma_above_lma = true;
        return Signal::BUY;
    }
    else if (prev_sma_above_lma && SMA < LMA) {
        prev_sma_above_lma = false;
        return Signal::SELL;
    }
    return Signal::HOLD;
}

double MACrossStrategy::calculateSMA() {
    double SMA = 0.0;
    for (int i = cb.size() - 1; i >= cb.size() - SMA_DAYS; i--) SMA += cb[i].price;
    SMA = SMA / SMA_DAYS;
    return SMA;
}

double MACrossStrategy::calculateLMA() {
    double LMA = 0.0;
    for (int i = 0; i < cb.size(); i++) LMA += cb[i].price;
    LMA = LMA / LMA_DAYS;
    return LMA;
}
