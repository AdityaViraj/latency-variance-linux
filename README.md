# latency-variance-linux

Goal: demonstrate why **tail latency** matters and how Linux/system effects create latency spikes even when average latency looks fine.

## What it measures
A tiny C++ loop measures per-iteration latency (ns) and reports:
- min / avg / p50 / p90 / p99 / p99.9 / max

## Build & run
```bash
g++ -O2 -std=c++20 -march=native -Wall -Wextra -pedantic -o latency src/main.cpp
./latency
