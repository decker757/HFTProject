#pragma once
#include <boost/lockfree/spsc_queue.hpp>
#include "market_data/Trade.h"

using TradeQueue = boost::lockfree::spsc_queue<Trade, boost::lockfree::capacity<1024>>;
