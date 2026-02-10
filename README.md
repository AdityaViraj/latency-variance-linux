# latency-variance-linux

Tail-latency microbenchmark in C++ demonstrating how Linux/system effects
(syscalls, page faults) create latency spikes and jitter even when average
latency looks fine.

This project focuses on predictability, not just raw speed — a core concern
in HFT, low-latency trading, and performance-critical systems.

---

## Why this matters ?

In real systems:

• Average latency can look excellent while p99 / p99.9 explodes  
• Small, rare events (syscalls, page faults, scheduler activity) dominate risk  
• Benchmarks that only report averages lie  

HFT systems optimize for:

• Tail latency  
• Variance reduction  
• Predictability of hot paths  

This repo demonstrates that problem with a minimal, reproducible experiment.

---

## What this project measures

A tiny, intentionally simple C++ hot loop measures per-iteration latency (ns)
and reports:

• min  
• avg  
• p50  
• p90  
• p99  
• p99.9  
• max  

Three scenarios are compared:

baseline   → warm, stable hot path  
syscall    → kernel boundary / syscall jitter  
pagefault  → cold memory & page-fault spikes  

---

## Build

Requires:
• Linux or macOS  
• g++ (C++20)

Using Makefile:

make

Manual build:

g++ -O2 -std=c++20 -march=native -Wall -Wextra -pedantic \
    -o latency src/main.cpp

---

## Run experiments

Baseline (warm, stable path):

./latency baseline

Syscall-induced jitter:

./latency syscall

Page-fault / cold-memory effects:

./latency pagefault

Multi-trial run (recommended):

./scripts/run.sh

Results are stored in results/.

---

## Sample results (excerpt)

Baseline:
avg:   ~26 ns  
p99:   ~42 ns  
max:   ~22 µs  

Syscall:
avg:   ~26 ns  
p99:   ~42 ns  
max:   ~10 µs  

Page fault:
avg:   ~2.1 µs  
p99:   ~6.4 µs  
p99.9: ~21 µs  
max:   >300 µs  

Key insight:
Average latency barely changes — tail latency explodes.

---

## What to look at

• Ignore avg  
• Focus on p99 / p99.9 / max  
• Rare events dominate worst-case behavior  

This is why HFT systems:

• pre-touch memory  
• avoid syscalls in hot paths  
• lock memory  
• pin threads and CPUs  

---

## Design choices

• std::chrono::steady_clock for monotonic timing  
• Warm-up phase to stabilize I-cache and branch predictors  
• volatile sink to prevent dead-code elimination  
• Minimal workload to isolate system effects  

---

## Limitations

• Not a trading system  
• Not measuring network latency  
• macOS results differ from Linux (expected)  

This is a conceptual microbenchmark, not a production profiler.

---

## Future experiments

• CPU pinning (taskset)  
• IRQ affinity  
• mlockall vs pageable memory  
• NUMA locality  
• perf stat / perf record  
• Linux vs tuned kernel comparison  

---

## Takeaway

Latency is not the problem. Variance is.  
Systems fail at the tail, not the mean.

This repo shows why predictability beats speed in real low-latency systems.

---

## License

MIT
