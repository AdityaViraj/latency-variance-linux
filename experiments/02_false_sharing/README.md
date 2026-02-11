# Experiment 02 — False Sharing (Cache Line Ping-Pong)

## Objective

Demonstrate **false sharing**: two threads update different variables, but performance collapses because those variables occupy the **same cache line**, causing unnecessary cache coherence traffic.

This experiment shows why data layout matters in performance-critical systems and why multi-threading can become unpredictable if memory is not structured carefully.

---

## What is False Sharing?

Modern CPUs move memory in fixed-size chunks called **cache lines** (typically 64 bytes).

If two threads modify different variables that reside on the **same cache line**, the cache line must repeatedly move between cores (MESI protocol).  
This creates:

- Cache line bouncing  
- Pipeline stalls  
- Increased memory traffic  
- Reduced throughput  
- Unpredictable performance  

Even though the threads are touching different variables, they interfere due to cache line ownership.

---

## Experiment Design

We compare two cases:

### CASE A — Packed Atomics (Likely Same Cache Line)

Two atomic counters are placed next to each other in memory:

```cpp
struct CountersPacked {
    std::atomic<uint64_t> a;
    std::atomic<uint64_t> b;
};
```

Both may reside on the same cache line → likely false sharing.

---

### CASE B — Padded Atomics (Separate Cache Lines)

Each atomic is aligned and padded to 64 bytes:

```cpp
struct alignas(64) PaddedAtomic {
    std::atomic<uint64_t> x;
    char pad[64 - sizeof(std::atomic<uint64_t>)]{};
};
```

Each counter gets its own cache line → avoids bouncing.

---

## What the Program Does

- Launches 2 threads
- Each thread increments its own counter
- Runs for a fixed duration (default 2 seconds)
- Measures total increments and calculates ops/sec

Higher ops/sec = better throughput.

---

## Build & Run

```bash
g++ -O2 -std=c++20 -march=native -Wall -Wextra -pedantic main.cpp -o false_sharing
./false_sharing 2
```

---

## Expected Results

Typically:

- CASE A (packed) → Lower ops/sec
- CASE B (padded) → Higher ops/sec

Why?

Because CASE A suffers from cache line bouncing, while CASE B gives each thread exclusive cache ownership.

---

## Systems Insight

This experiment demonstrates:

- Memory layout can dominate performance
- False sharing reduces scalability
- Cache coherence traffic creates hidden latency
- Multithreading requires careful data alignment

In low-latency systems (e.g., trading engines):

- False sharing can create unpredictable tail latency
- Data structure layout is as important as algorithms
- Thread/core ownership must be controlled

---

## Key Takeaway

The problem is not "threads are slow."

The problem is:

> Threads sharing cache lines are slow.

Careful memory alignment and data separation can dramatically improve performance predictability.

---
