#pragma once
#include "market_data/Trade.h"
#include "core/Signal.h"

class StrategyBase {
private:
    

public:
    virtual Signal update(Trade) = 0;
    virtual ~StrategyBase() {};
};