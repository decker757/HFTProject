#include "strategy/StrategyEvaluator.h"
#include <numeric>
#include <cmath>

StrategyEvaluator::StrategyEvaluator(std::unordered_map<StrategyType, StrategyBase*> strategies) : strategies_(strategies) {}

StrategyBase* StrategyEvaluator::select(Trade t) {
    prices_.push_back(t.price);
    if (prices_.size() > period_) prices_.pop_front();
    if (prices_.size() < period_) return strategies_[StrategyType::MA_CROSS]; // default

    double mean = std::accumulate(prices_.begin(), prices_.end(), 0.0) / period_;
    double variance = 0.0;
    for (double p : prices_) variance += (p - mean) * (p - mean);
    double stddev = std::sqrt(variance / period_);
    double cv = stddev / mean; // coefficient of variation

    if (cv > 0.02) return strategies_[StrategyType::MOMENTUM];
    
    // check if trending or ranging via mean deviation
    double latest = prices_.back();
    double deviation = std::abs(latest - mean) / mean;

    if (deviation > 0.01) return strategies_[StrategyType::MA_CROSS];
    if (deviation > 0.005) return strategies_[StrategyType::BOLLINGER];
    return strategies_[StrategyType::RSI];
}