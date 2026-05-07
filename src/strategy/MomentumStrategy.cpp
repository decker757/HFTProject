#include "strategy/MomentumStrategy.h"

Signal MomentumStrategy::update(Trade t) {
    prices_.push_back(t.price);
    if (prices_.size() > period_) prices_.pop_front();
    if (prices_.size() < period_) return Signal::HOLD;

    double change = (prices_.back() - prices_.front()) / prices_.front();

    if (change > threshold_) return Signal::BUY;
    if (change < -threshold_) return Signal::SELL;
    return Signal::HOLD;
}