#pragma once

#include "market_data/Trade.h"
#include "strategy/StrategyBase.h"

class StrategyEvaluatorBase {
public:
    virtual StrategyBase* select(Trade t) = 0;
    virtual ~StrategyEvaluatorBase() = default;
};