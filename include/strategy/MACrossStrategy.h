#pragma once

#include <boost/circular_buffer.hpp>

#include "StrategyBase.h"
#include "core/Signal.h"


class MACrossStrategy : public StrategyBase {
private:
    bool prev_sma_above_lma;
    boost::circular_buffer<Trade> cb;

public:
    MACrossStrategy();
    double calculateSMA();
    double calculateLMA();
    Signal update(Trade) override;
};