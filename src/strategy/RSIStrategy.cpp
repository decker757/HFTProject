#include "strategy/RSIStrategy.h"
#include <cmath>

Signal RSIStrategy::update(Trade t) {
    prices_.push_back(t.price);
    if (prices_.size() > period_ + 1) prices_.pop_front();
    if (prices_.size() < period_ + 1) return Signal::HOLD;

    if (!initialized_) {
        double gains = 0.0, losses = 0.0;
        for (int i = 1; i <= period_; i++) {
            double change = prices_[i] - prices_[i-1];
            if (change > 0) gains += change;
            else losses += std::abs(change);
        }
        avg_gain_ = gains / period_;
        avg_loss_ = losses / period_;
        initialized_ = true;
    } else {
        double change = prices_.back() - prices_[prices_.size()-2];
        double gain = change > 0 ? change : 0.0;
        double loss = change < 0 ? std::abs(change) : 0.0;
        avg_gain_ = (avg_gain_ * (period_ - 1) + gain) / period_;
        avg_loss_ = (avg_loss_ * (period_ - 1) + loss) / period_;
    }

    if (avg_loss_ == 0) return Signal::HOLD;
    double rs = avg_gain_ / avg_loss_;
    double rsi = 100.0 - (100.0 / (1.0 + rs));

    if (rsi <= oversold_) return Signal::BUY;
    if (rsi >= overbought_) return Signal::SELL;
    return Signal::HOLD;
}