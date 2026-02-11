# Experiment 01 — Stack vs Heap (Cache-Dominated Benchmark)

## Objective

Investigate whether stack allocation is actually faster than heap allocation, and understand what this type of microbenchmark truly measures.

This experiment demonstrates how CPU cache behavior dominates performance and why naive conclusions about stack vs heap can be misleading.

---

## Experiment Setup

We compare two cases:

1. Stack-allocated array  
2. Heap-allocated array  

Both arrays:

- Size: 1000 integers (~4 KB)
- Accessed inside a tight loop
- Total iterations: 10,000,000
- Timing measured using `std::chrono`

Code structure:

```
int stack_arr[1000];           // Stack allocation
int* heap_arr = new int[1000]; // Heap allocation
```

Loop:

```
for (int i = 0; i < N; i++) {
    arr[i % 1000]++;
}
```

---

## Example Output

```
Stack time: 18 ms
Heap time: 10 ms
```

At first glance, heap appears faster.

But this conclusion is incorrect.

---

## What This Benchmark Actually Measures

### 1. Allocation Cost Is NOT Measured

Both allocations happen once:

```
int stack_arr[1000];
new int[1000];
```

The loop measures memory access performance, not allocation time.

---

### 2. Cache Behavior Dominates

1000 integers ≈ 4 KB  
This fits entirely inside L1 cache.

After initial warmup:

- Both arrays reside in L1
- Access latency becomes extremely low
- Stack vs heap location becomes irrelevant

The CPU is executing repeated L1 cache hits.

---

### 3. Why Heap May Appear Faster

Possible reasons:

- Stack memory is uninitialized (undefined behavior)
- Heap memory may be zeroed or reused from allocator cache
- Compiler optimizations may treat them differently
- Pipeline effects and data dependencies influence timing

This is a classic example of misleading microbenchmarks.

---

## Systems Lessons

Incorrect takeaway:
Heap is faster than stack.

Correct takeaways:
- Cache locality dominates performance.
- Allocation type does not matter once data is cached.
- Microbenchmarks must be carefully designed.
- Initialization and undefined behavior affect results.
- Modern CPU behavior is complex and non-obvious.

---

## Build & Run

From this directory:

```
g++ -O2 -std=c++20 -march=native -Wall -Wextra -pedantic main.cpp -o stack_vs_heap
./stack_vs_heap
```

---

## Why This Matters

In low-latency systems :

- Cache behavior is critical.
- Benchmarks must reflect real workloads.
- Incorrect conclusions can lead to poor architectural decisions.
- Tail latency and predictability matter more than simple averages.

---

## Future Improvements

To make this experiment more rigorous:

- Explicitly initialize both arrays
- Separate allocation timing from access timing
- Increase array size beyond L3 cache
- Measure hardware counters using `perf`
- Pin process to a single core
- Disable CPU frequency scaling

---

## Conclusion

This experiment is not about stack vs heap.

It is about understanding how modern CPUs, caches, and compilers actually behave.


## Sample Results (macOS, clang -O2)

Stack time: 18 ms
Heap time: 10 ms

Note:
These results are cache-dominated and do NOT measure allocation cost.
See explanation above.
