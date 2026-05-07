#pragma once

#include "StrategyBase.h"
#include "core/Signal.h"
#include "market_data/Trade.h"
#include <deque>

class MomentumStrategy : public StrategyBase {
private:
    int period_ = 20;
    double threshold_ = 0.002; // 0.02% price change
    std::deque<double> prices_;
    
public:
    Signal update(Trade t) override;
};