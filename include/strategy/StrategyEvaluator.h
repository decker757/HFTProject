#pragma once

#include "strategy/StrategyEvaluatorBase.h"
#include "market_data/Trade.h"
#include <deque>
#include <unordered_map>

enum class StrategyType { MA_CROSS, BOLLINGER, MOMENTUM, RSI };

class StrategyEvaluator : public StrategyEvaluatorBase {
    std::unordered_map<StrategyType, StrategyBase*> strategies_;
    std::deque<double> prices_;
    int period_ = 20;
public:
    StrategyEvaluator(std::unordered_map<StrategyType, StrategyBase*> strategies);
    StrategyBase* select(Trade t) override;
};