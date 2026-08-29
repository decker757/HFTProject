#pragma once
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>

#include "core/LatencyHistogram.h"
#include "core/Signal.h"
#include "execution/ExecutionEngine.h"
#include "market_data/Trade.h"

/*
    Takes the order round trip off the strategy threads.

    Before: every strategy thread called ExecutionEngine::executeOrder directly,
    which takes a mutex and holds it across a blocking HTTPS write+read to
    Binance. One BTC order therefore stalled the ETH, SOL and XRP strategies for
    a full internet round trip, and their market-data queues kept filling while
    they were parked.

    After: strategies hand the signal to a bounded queue and go straight back to
    reading trades. A single sender thread owns the ExecutionEngine.

    Why a mutex and condition variable rather than a lock-free queue: this path
    is throttled to one order per symbol per second and the consumer blocks on a
    ~40 ms network round trip. An uncontended lock is ~100 ns and a condvar
    burns nothing while idle; spinning on a lock-free queue would cost a core to
    save nanoseconds on a millisecond-scale path. The lock-free queue earns its
    keep on market data, where messages arrive continuously and the consumer
    never blocks.
*/

struct OrderRequest {
    Signal   signal = Signal::HOLD;
    char     symbol[kMaxSymbolLen + 1] = {};
    uint64_t enqueued_ns = 0;
};

class OrderRouter {
public:
    OrderRouter(ExecutionEngine& exec, std::size_t capacity = 256);
    ~OrderRouter();

    OrderRouter(const OrderRouter&) = delete;
    OrderRouter& operator=(const OrderRouter&) = delete;

    void start();
    void stop();   // wakes the sender, lets it drain, joins

    // Never blocks on the network. Returns false if the queue was full,
    // which means the order was dropped rather than queued behind a backlog.
    bool submit(Signal s, const char* symbol, uint64_t enqueued_ns);

    uint64_t submitted() const { return submitted_.load(std::memory_order_relaxed); }
    uint64_t sent()      const { return sent_.load(std::memory_order_relaxed); }
    uint64_t dropped()   const { return dropped_.load(std::memory_order_relaxed); }

    // Only safe to read after stop() has joined the sender thread.
    LatencyHistogram& queueWait() { return queueWait_; }
    LatencyHistogram& sendTime()  { return sendTime_; }

private:
    void run();

    ExecutionEngine& exec_;
    std::size_t capacity_;

    std::mutex mtx_;
    std::condition_variable cv_;
    std::deque<OrderRequest> queue_;
    bool stopping_ = false;

    std::thread worker_;
    std::atomic<uint64_t> submitted_{0};
    std::atomic<uint64_t> sent_{0};
    std::atomic<uint64_t> dropped_{0};

    LatencyHistogram queueWait_{1u << 16};
    LatencyHistogram sendTime_{1u << 16};
};
