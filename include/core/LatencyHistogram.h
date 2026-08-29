#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

/*
    Records nanosecond durations into a preallocated buffer so the hot path
    never allocates. Percentiles are computed off-line at report time.

    One instance per thread. Nothing here is thread-safe by design: sharing it
    would put a lock on the path we are trying to measure.
*/
class LatencyHistogram {
public:
    explicit LatencyHistogram(std::size_t capacity = 1u << 20) : capacity_(capacity) {
        samples_.reserve(capacity_);
    }

    // Hot path: no allocation once capacity is reached, oldest samples are kept.
    inline void record(uint64_t ns) {
        if (samples_.size() < capacity_) samples_.push_back(ns);
        else ++overflow_;
    }

    std::size_t count() const { return samples_.size(); }
    uint64_t overflowed() const { return overflow_; }
    bool empty() const { return samples_.empty(); }

    // Sorts in place. Call once, at report time.
    uint64_t percentile(double p) {
        if (samples_.empty()) return 0;
        if (!sorted_) { std::sort(samples_.begin(), samples_.end()); sorted_ = true; }
        double idx = (p / 100.0) * static_cast<double>(samples_.size() - 1);
        std::size_t i = static_cast<std::size_t>(idx + 0.5);
        return samples_[std::min(i, samples_.size() - 1)];
    }

    uint64_t min()  { return percentile(0.0); }
    uint64_t max()  { return percentile(100.0); }
    double   mean() const {
        if (samples_.empty()) return 0.0;
        long double sum = 0;
        for (uint64_t v : samples_) sum += v;
        return static_cast<double>(sum / samples_.size());
    }

    std::string report(const std::string& label);

    void merge(const LatencyHistogram& other) {
        samples_.insert(samples_.end(), other.samples_.begin(), other.samples_.end());
        overflow_ += other.overflow_;
        sorted_ = false;
    }

private:
    std::vector<uint64_t> samples_;
    std::size_t capacity_;
    uint64_t overflow_ = 0;
    bool sorted_ = false;
};
