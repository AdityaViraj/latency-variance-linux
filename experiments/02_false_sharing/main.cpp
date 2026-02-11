#include <atomic>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>

using Clock = std::chrono::steady_clock;

static uint64_t ns_since(const Clock::time_point& a, const Clock::time_point& b) {
    return (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(b - a).count();
}

// Two counters that may sit on the same cache line => false sharing.
struct CountersPacked {
    std::atomic<uint64_t> a{0};
    std::atomic<uint64_t> b{0};
};

// Force counters onto different cache lines (typical 64B line).
struct alignas(64) PaddedAtomic {
    std::atomic<uint64_t> x{0};
    char pad[64 - sizeof(std::atomic<uint64_t>)]{};
};

struct CountersPadded {
    PaddedAtomic a;
    PaddedAtomic b;
};

template <typename Counters>
static uint64_t run_case(const char* label, int seconds) {
    Counters c;

    std::atomic<bool> start{false};
    std::atomic<bool> stop{false};

    auto workerA = [&] {
        while (!start.load(std::memory_order_acquire)) {}
        while (!stop.load(std::memory_order_relaxed)) {
            c.a.x.fetch_add(1, std::memory_order_relaxed);
        }
    };

    auto workerB = [&] {
        while (!start.load(std::memory_order_acquire)) {}
        while (!stop.load(std::memory_order_relaxed)) {
            c.b.x.fetch_add(1, std::memory_order_relaxed);
        }
    };

    // Support both layouts: packed has fields a/b directly, padded has a.x/b.x.
    // We'll adapt with lambdas below.
    // (We avoid templates + if constexpr for simplicity.)

    // Start threads
    std::thread t1, t2;

    auto t0 = Clock::now();
    if constexpr (std::is_same_v<Counters, CountersPacked>) {
        auto w1 = [&] {
            while (!start.load(std::memory_order_acquire)) {}
            while (!stop.load(std::memory_order_relaxed)) {
                c.a.fetch_add(1, std::memory_order_relaxed);
            }
        };
        auto w2 = [&] {
            while (!start.load(std::memory_order_acquire)) {}
            while (!stop.load(std::memory_order_relaxed)) {
                c.b.fetch_add(1, std::memory_order_relaxed);
            }
        };
        t1 = std::thread(w1);
        t2 = std::thread(w2);
    } else {
        t1 = std::thread(workerA);
        t2 = std::thread(workerB);
    }

    start.store(true, std::memory_order_release);

    std::this_thread::sleep_for(std::chrono::seconds(seconds));
    stop.store(true, std::memory_order_release);

    t1.join();
    t2.join();
    auto t1_end = Clock::now();

    uint64_t total = 0;
    if constexpr (std::is_same_v<Counters, CountersPacked>) {
        total = c.a.load(std::memory_order_relaxed) + c.b.load(std::memory_order_relaxed);
    } else {
        total = c.a.x.load(std::memory_order_relaxed) + c.b.x.load(std::memory_order_relaxed);
    }

    uint64_t dt_ns = ns_since(t0, t1_end);
    double ops_per_sec = (double)total / ((double)dt_ns / 1e9);

    std::cout << label << "\n";
    std::cout << "  total ops: " << total << "\n";
    std::cout << "  ops/sec : " << std::fixed << std::setprecision(2) << ops_per_sec << "\n\n";

    return total;
}

int main(int argc, char** argv) {
    int seconds = 2;
    if (argc >= 2) seconds = std::max(1, std::atoi(argv[1]));

    std::cout << "Experiment 02 — False Sharing (cache line ping-pong)\n";
    std::cout << "Run time: " << seconds << "s\n\n";

    run_case<CountersPacked>("CASE A: packed atomics (likely SAME cache line)  => false sharing", seconds);
    run_case<CountersPadded>("CASE B: padded atomics (separate cache lines)    => less bouncing", seconds);

    std::cout << "Expectation: CASE B should usually have higher ops/sec.\n";
    return 0;
}
