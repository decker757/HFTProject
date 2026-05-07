#pragma once

#include "StrategyBase.h"
#include "core/Signal.h"
#include "market_data/Trade.h"
#include <deque>

class RSIStrategy : public StrategyBase {
private:
    int period_ = 20;
    double overbought_ = 70.0;
    double oversold_ = 30.0;
    double avg_gain_ = -1.0;
    double avg_loss_ = -1.0;
    bool initialized_ = false;
    std::deque<double> prices_;
    
public:
    Signal update(Trade t) override;
};