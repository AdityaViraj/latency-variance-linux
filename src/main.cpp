#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

#include <unistd.h>   // getpid()
#include <sys/mman.h> // mmap, munmap, madvise

using Clock = std::chrono::steady_clock;

static uint64_t percentile_ns(std::vector<uint64_t> v, double p) {
    if (v.empty()) return 0;
    std::sort(v.begin(), v.end());
    double idx = p * (v.size() - 1);
    size_t i = (size_t)idx;
    return v[i];
}

static void print_stats(const std::vector<uint64_t>& samples) {
    auto [mn_it, mx_it] = std::minmax_element(samples.begin(), samples.end());
    long double avg = std::accumulate(samples.begin(), samples.end(), (long double)0.0) / samples.size();

    uint64_t p50  = percentile_ns(samples, 0.50);
    uint64_t p90  = percentile_ns(samples, 0.90);
    uint64_t p99  = percentile_ns(samples, 0.99);
    uint64_t p999 = percentile_ns(samples, 0.999);

    std::cout << "Latency (ns) per iteration\n";
    std::cout << "min:  " << *mn_it << "\n";
    std::cout << "avg:  " << std::fixed << std::setprecision(2) << (double)avg << "\n";
    std::cout << "p50:  " << p50 << "\n";
    std::cout << "p90:  " << p90 << "\n";
    std::cout << "p99:  " << p99 << "\n";
    std::cout << "p99.9:" << p999 << "\n";
    std::cout << "max:  " << *mx_it << "\n";
}

// Baseline: tiny hot-path workload (mostly measures timing overhead + CPU noise)
static inline void work_baseline(volatile uint64_t& sink) {
    sink += (sink << 1) ^ 0x9e3779b97f4a7c15ULL;
}

// Syscall mode: add a syscall each iter to show jitter cost
static inline void work_syscall(volatile uint64_t& sink) {
    sink += (uint64_t)getpid(); // syscall
}

// Pagefault mode: touch a new page each iter (major spikes early, then stable)
static void work_pagefault(size_t iters, std::vector<uint64_t>& samples) {
    const size_t page = (size_t)sysconf(_SC_PAGESIZE);
    const size_t pages = iters;                 // 1 page per iter
    const size_t bytes = pages * page;

    void* mem = mmap(nullptr, bytes, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANON, -1, 0);
    if (mem == MAP_FAILED) {
        std::cerr << "mmap failed\n";
        std::exit(1);
    }

    // Hint kernel we will access sequentially (best-effort)
    madvise(mem, bytes, MADV_SEQUENTIAL);

    volatile uint8_t* p = (volatile uint8_t*)mem;

    for (size_t i = 0; i < iters; i++) {
        auto t0 = Clock::now();
        p[i * page] = (uint8_t)(p[i * page] + 1);          // first touch => page fault the first time
        auto t1 = Clock::now();
        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
        samples.push_back((uint64_t)ns);
    }

    munmap(mem, bytes);
}

int main(int argc, char** argv) {
    std::string mode = (argc >= 2) ? argv[1] : "baseline";

    const size_t WARMUP_ITERS = 50'000;
    const size_t ITERS = 1'000'000;

    // Warmup: reduce one-time effects (ICache, branch predictor, etc.)
    volatile uint64_t sink = 0;
    for (size_t i = 0; i < WARMUP_ITERS; i++) {
        sink += (uint64_t)i * 1315423911ULL;
    }

    std::vector<uint64_t> samples;
    samples.reserve(ITERS);

    if (mode == "pagefault") {
        // smaller iters to avoid huge mmap on laptops
        const size_t PF_ITERS = 200'000;
        samples.reserve(PF_ITERS);
        work_pagefault(PF_ITERS, samples);
        print_stats(samples);
    } else {
        for (size_t i = 0; i < ITERS; i++) {
            auto t0 = Clock::now();

            if (mode == "baseline") {
                work_baseline(sink);
            } else if (mode == "syscall") {
                work_syscall(sink);
            } else {
                std::cerr << "Unknown mode: " << mode << "\n";
                std::cerr << "Usage: ./latency [baseline|syscall|pagefault]\n";
                return 2;
            }

            auto t1 = Clock::now();
            auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
            samples.push_back((uint64_t)ns);
        }

        print_stats(samples);
    }

    // Prevent optimizing away (silent)
    static volatile uint64_t guard = 0;
    guard ^= sink;
    return 0;
}
