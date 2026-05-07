#pragma once

#include "StrategyBase.h"
#include "core/Signal.h"
#include "market_data/Trade.h"
#include <deque>

class BollingerBandsStrategy : public StrategyBase {
private:
    int period_ = 20;
    double multiplier_ = 2.0;
    std::deque<double> prices_;
    
public:
    Signal update(Trade t) override;
};