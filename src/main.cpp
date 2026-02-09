#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <vector>

using clock_t = std::chrono::steady_clock;

static uint64_t percentile_ns(std::vector<uint64_t> v, double p) {
    if (v.empty()) return 0;
    std::sort(v.begin(), v.end());
    const double idx = p * (v.size() - 1);
    const size_t i = static_cast<size_t>(idx);
    return v[i];
}

int main() {
    // Baseline config: keep it simple and stable.
    const int WARMUP_ITERS = 50'000;
    const int ITERS        = 1'000'000;

    // A tiny “hot-path” workload to measure (kept small on purpose).
    volatile uint64_t sink = 0;

    // Warmup: reduce one-time effects (ICache, branch predictor, etc.)
    for (int i = 0; i < WARMUP_ITERS; i++) {
        sink += static_cast<uint64_t>(i) * 1315423911ull;
    }

    std::vector<uint64_t> samples;
    samples.reserve(ITERS);

    for (int i = 0; i < ITERS; i++) {
        auto t0 = clock_t::now();

        // "Work" (intentionally tiny)
        sink += (sink << 1) ^ 0x9e3779b97f4a7c15ull;

        auto t1 = clock_t::now();
        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
        samples.push_back(static_cast<uint64_t>(ns));
    }

    // Basic stats
    auto min_it = std::min_element(samples.begin(), samples.end());
    auto max_it = std::max_element(samples.begin(), samples.end());
    long double avg = std::accumulate(samples.begin(), samples.end(), (long double)0.0) / samples.size();

    uint64_t p50  = percentile_ns(samples, 0.50);
    uint64_t p90  = percentile_ns(samples, 0.90);
    uint64_t p99  = percentile_ns(samples, 0.99);
    uint64_t p999 = percentile_ns(samples, 0.999);

    std::cout << "Latency (ns) per iteration\n";
    std::cout << "min:  " << *min_it << "\n";
    std::cout << "avg:  " << std::fixed << std::setprecision(2) << (double)avg << "\n";
    std::cout << "p50:  " << p50 << "\n";
    std::cout << "p90:  " << p90 << "\n";
    std::cout << "p99:  " << p99 << "\n";
    std::cout << "p99.9:" << p999 << "\n";
    std::cout << "max:  " << *max_it << "\n";

    // Prevent optimizing away
    std::cerr << "sink=" << sink << "\n";
    return 0;
}
