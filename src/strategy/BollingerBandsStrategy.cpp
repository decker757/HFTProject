#include "strategy/BollingerBandsStrategy.h"
#include <cmath>
#include <numeric>

Signal BollingerBandsStrategy::update(Trade t) {
    prices_.push_back(t.price);
    if (prices_.size() > period_) prices_.pop_front();
    if (prices_.size() < period_) return Signal::HOLD;

    double mean = std::accumulate(prices_.begin(), prices_.end(), 0.0) / period_;

    double variance = 0.0;
    for (double p : prices_) variance += (p - mean) * (p - mean);
    double stddev = std::sqrt(variance / period_);

    double upper = mean + multiplier_ * stddev;
    double lower = mean - multiplier_ * stddev;

    if (t.price <= lower) return Signal::BUY;
    if (t.price >= upper) return Signal::SELL;
    return Signal::HOLD;
}