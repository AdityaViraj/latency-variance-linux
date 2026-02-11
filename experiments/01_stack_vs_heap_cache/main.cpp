#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <vector>

using Clock = std::chrono::steady_clock;

// ------------------------------------------------------------
// PURPOSE
// ------------------------------------------------------------
// This is NOT a "stack vs heap speed" benchmark.
// It demonstrates:
//
// 1) Allocation cost != access cost
// 2) Cache dominates performance
// 3) Initialization matters
// 4) Microbenchmarks can mislead
//
// We measure memory access latency for small arrays
// that fit entirely inside CPU cache.
//
// ------------------------------------------------------------

constexpr int ARRAY_SIZE = 1024;
constexpr int ITERATIONS = 10'000'000;

volatile uint64_t sink = 0;

// Simple timing wrapper
template <typename F>
uint64_t measure(F&& func) {
    auto start = Clock::now();
    func();
    auto end = Clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
}

int main() {
    std::cout << "=== Stack vs Heap (Cache-Dominated Experiment) ===\n\n";

    // ------------------------------------------------------------
    // Stack allocation
    // ------------------------------------------------------------
    int stack_arr[ARRAY_SIZE] = {};  // IMPORTANT: initialize to avoid UB

    uint64_t stack_time = measure([&]() {
        for (int i = 0; i < ITERATIONS; i++) {
            stack_arr[i % ARRAY_SIZE]++;
        }
    });

    // ------------------------------------------------------------
    // Heap allocation
    // ------------------------------------------------------------
    int* heap_arr = new int[ARRAY_SIZE](); // zero-initialize

    uint64_t heap_time = measure([&]() {
        for (int i = 0; i < ITERATIONS; i++) {
            heap_arr[i % ARRAY_SIZE]++;
        }
    });

    delete[] heap_arr;

    std::cout << "Stack access time: "
              << stack_time / 1e6 << " ms\n";

    std::cout << "Heap access time:  "
              << heap_time / 1e6 << " ms\n";

    std::cout << "\nNOTE:\n";
    std::cout << "This test measures memory access speed,\n";
    std::cout << "NOT allocation cost.\n\n";

    std::cout << "Because the arrays are small (~4KB),\n";
    std::cout << "they fit entirely in L1 cache.\n\n";

    std::cout << "Once cached, stack vs heap placement\n";
    std::cout << "does NOT matter.\n\n";

    std::cout << "Cache dominates performance.\n";
    std::cout << "Microbenchmarks can mislead.\n";

    sink += stack_arr[0]; // prevent optimization
}
