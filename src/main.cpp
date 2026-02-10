#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

#include <unistd.h>   // getpid(), sysconf()

// -----------------------------
// Core idea of this program
// -----------------------------
// - avg might look good
// - but rare events create spikes (p99/p99.9/max)
// - those spikes are what hurt low-latency systems like the ones in HFTs
//
// We measure: min / avg / p50 / p90 / p99 / p99.9 / max across many iterations of a tiny HOT PATH.

using Clock = std::chrono::steady_clock;

// -----------------------------
// Helpers: percentiles + stats
// -----------------------------

static uint64_t percentile_sorted(const std::vector<uint64_t>& sorted, double p) {
    // "sorted" must be sorted ascending.
    // p in [0, 1]. We use a simple nearest-rank-like index.
    if (sorted.empty()) return 0;
    if (p <= 0.0) return sorted.front();
    if (p >= 1.0) return sorted.back();

    const double idx = p * (sorted.size() - 1);
    const size_t i = static_cast<size_t>(idx);
    return sorted[i];
}

struct Stats {
    uint64_t min = 0;
    uint64_t max = 0;
    double   avg = 0.0;
    uint64_t p50 = 0;
    uint64_t p90 = 0;
    uint64_t p99 = 0;
    uint64_t p999 = 0;
};

static Stats compute_stats(std::vector<uint64_t> samples) {
    // We copy samples in so we can sort it freely.
    // Sorting is done AFTER measurement, so it does NOT affect timings.
    Stats s;
    if (samples.empty()) return s;

    auto [mn_it, mx_it] = std::minmax_element(samples.begin(), samples.end());
    s.min = *mn_it;
    s.max = *mx_it;

    // Average (mean) can hide spikes; still report it, but do not trust it alone.
    long double sum = std::accumulate(samples.begin(), samples.end(),
                                      (long double)0.0);
    s.avg = (double)(sum / (long double)samples.size());

    std::sort(samples.begin(), samples.end());
    s.p50  = percentile_sorted(samples, 0.50);
    s.p90  = percentile_sorted(samples, 0.90);
    s.p99  = percentile_sorted(samples, 0.99);
    s.p999 = percentile_sorted(samples, 0.999);

    return s;
}

static void print_stats(const Stats& s) {
    std::cout << "Latency (ns) per iteration\n";
    std::cout << "min:   " << s.min << "\n";
    std::cout << "avg:   " << std::fixed << std::setprecision(2) << s.avg << "\n";
    std::cout << "p50:   " << s.p50 << "\n";
    std::cout << "p90:   " << s.p90 << "\n";
    std::cout << "p99:   " << s.p99 << "\n";
    std::cout << "p99.9: " << s.p999 << "\n";
    std::cout << "max:   " << s.max << "\n";
}

// -----------------------------
// Workload modes
// -----------------------------
// baseline: extremely tiny pure userspace work
// syscall:  same, but forces kernel boundary each iteration
// pagefault: forces first-touch of new pages inside the measured region
//
// THEORY:
// - syscall adds jitter because kernel entry/exit & scheduling effects
// - pagefault adds huge spikes because the OS has to map a new page
//   (fault handling, zero-fill, accounting, TLB updates, etc.)

enum class Mode { Baseline, Syscall, Pagefault };

static Mode parse_mode(int argc, char** argv) {
    if (argc < 2) return Mode::Baseline;
    std::string m = argv[1];

    if (m == "baseline") return Mode::Baseline;
    if (m == "syscall")  return Mode::Syscall;
    if (m == "pagefault") return Mode::Pagefault;

    // Default if user passes something unknown.
    return Mode::Baseline;
}

// -----------------------------
// Main benchmark runner
// -----------------------------

int main(int argc, char** argv) {
    const Mode mode = parse_mode(argc, argv);

    // Measurement settings
    // THEORY:
    // - warmup reduces first-time effects: instruction cache, branch predictor, etc.
    // - more iterations gives us a stable distribution to compute percentiles.
    const int WARMUP_ITERS = 50'000;
    const int ITERS        = 1'000'000;

    // volatile prevents compiler from optimizing away our "work".
    volatile uint64_t sink = 0;

    // Warmup phase (NOT measured)
    for (int i = 0; i < WARMUP_ITERS; i++) {
        sink += (uint64_t)i * 1315423911ull;
    }

    // For pagefault mode we allocate a buffer but DO NOT touch it yet.
    // THEORY:
    // - Allocation does not necessarily touch physical pages immediately.
    // - The first write to a new page can trigger a (minor) page fault.
    const long page_size = sysconf(_SC_PAGESIZE);
    std::vector<uint8_t> page_buf;

    // Choose how many pages to use for pagefault demo.
    // Make it not too huge so it runs fast, but large enough to show spikes.
    const size_t PF_PAGES = 4096; // ~16MB if pages are 4KB

    if (mode == Mode::Pagefault) {
        page_buf.resize(PF_PAGES * (size_t)page_size);
        // Intentionally DO NOT memset / touch now.
    }

    std::vector<uint64_t> samples;
    samples.reserve(ITERS);

    // Benchmark loop (MEASURED)
    for (int i = 0; i < ITERS; i++) {
        const auto t0 = Clock::now();

        // -----------------------------
        // HOT PATH work starts here
        // -----------------------------
        // Thinking: this is the part we want predictable.
        // Anything that triggers OS activity here can cause spikes.

        if (mode == Mode::Baseline) {
            // Tiny arithmetic; stays in user-space.
            sink ^= (sink << 1) + 0x9e3779b97f4a7c15ull;
        }
        else if (mode == Mode::Syscall) {
            // Any syscall crosses user -> kernel -> user.
            // Even if "fast", it can introduce variability.
            (void)getpid();
            sink ^= (sink << 1) + 0x9e3779b97f4a7c15ull;
        }
        else { 
            // Mode::Pagefault
            // Force first-touch on a fresh page (write causes page fault on first use).
            // We cycle through pages; during early iterations many touches are "cold".
            const size_t page = (size_t)i % PF_PAGES;
            page_buf[page * (size_t)page_size]++; // first-touch => likely fault (initially)
            sink ^= (sink << 1) + 0x9e3779b97f4a7c15ull;
        }

        // -----------------------------
        // "Hot path" work ends here
        // -----------------------------

        const auto t1 = Clock::now();
        const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
        samples.push_back((uint64_t)ns);
    }

    // Compute stats (OFF hot path)
    const Stats s = compute_stats(samples);
    print_stats(s);

    // Keep sink alive (prevents aggressive optimization)
    std::cerr << "sink=" << sink << "\n";

    // NOTE:
    // On macOS, behaviour differs from Linux in many details (page faults, syscalls, scheduling).
    // The lesson still holds: tail latency is where the pain lives.
    return 0;
}
