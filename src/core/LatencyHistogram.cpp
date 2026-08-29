#include "core/LatencyHistogram.h"

#include <iomanip>
#include <sstream>

namespace {
std::string us(uint64_t ns) {
    std::ostringstream o;
    o << std::fixed << std::setprecision(3) << (static_cast<double>(ns) / 1000.0);
    return o.str();
}
}

std::string LatencyHistogram::report(const std::string& label) {
    std::ostringstream o;
    o << label << "\n";
    if (empty()) {
        o << "  (no samples)\n";
        return o.str();
    }
    o << "  samples " << count();
    if (overflow_) o << " (+" << overflow_ << " dropped, buffer full)";
    o << "\n";
    o << "  mean    " << us(static_cast<uint64_t>(mean())) << " us\n";
    o << "  p50     " << us(percentile(50.0))   << " us\n";
    o << "  p90     " << us(percentile(90.0))   << " us\n";
    o << "  p99     " << us(percentile(99.0))   << " us\n";
    o << "  p99.9   " << us(percentile(99.9))   << " us\n";
    o << "  max     " << us(max())              << " us\n";
    return o.str();
}
