#include "execution/OrderRouter.h"
#include "core/Logging.h"

#include <chrono>
#include <cstring>
#include <iostream>

namespace {
inline uint64_t nowNs() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
}
}

OrderRouter::OrderRouter(ExecutionEngine& exec, std::size_t capacity)
    : exec_(exec), capacity_(capacity) {}

OrderRouter::~OrderRouter() { stop(); }

void OrderRouter::start() {
    if (worker_.joinable()) return;
    worker_ = std::thread([this] { run(); });
}

void OrderRouter::stop() {
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (stopping_) return;
        stopping_ = true;
    }
    cv_.notify_all();
    if (worker_.joinable()) worker_.join();
}

bool OrderRouter::submit(Signal s, const char* symbol, uint64_t enqueued_ns) {
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (stopping_) return false;
        // Bounded on purpose. If Binance stalls we shed load and say so,
        // rather than growing a queue of orders the market has moved past.
        if (queue_.size() >= capacity_) {
            dropped_.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        OrderRequest req;
        req.signal = s;
        std::strncpy(req.symbol, symbol, kMaxSymbolLen);
        req.symbol[kMaxSymbolLen] = '\0';
        req.enqueued_ns = enqueued_ns;
        queue_.push_back(req);
    }
    submitted_.fetch_add(1, std::memory_order_relaxed);
    cv_.notify_one();
    return true;
}

void OrderRouter::run() {
    for (;;) {
        OrderRequest req;
        {
            std::unique_lock<std::mutex> lock(mtx_);
            cv_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
            if (queue_.empty()) {
                if (stopping_) return;   // drained
                continue;
            }
            req = queue_.front();
            queue_.pop_front();
        }

        const uint64_t dequeued = nowNs();
        queueWait_.record(dequeued - req.enqueued_ns);

        try {
            exec_.executeOrder(req.signal, req.symbol);
        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lock(coutMtx);
            std::cerr << "OrderRouter: send failed: " << e.what() << "\n";
        }

        sendTime_.record(nowNs() - dequeued);
        sent_.fetch_add(1, std::memory_order_relaxed);
    }
}
