# latency-variance-linux

Goal: demonstrate why **tail latency** matters and how Linux/system effects create latency spikes even when average latency looks fine.

## What it measures
A tiny C++ loop measures per-iteration latency (ns) and reports:
- min / avg / p50 / p90 / p99 / p99.9 / max

## Build & run
```bash
g++ -O2 -std=c++20 -march=native -Wall -Wextra -pedantic -o latency src/main.cpp
./latency


## Results
 
### baseline
Latency (ns) per iteration
min:  0
avg:  26.67
p50:  41
p90:  42
p99:  42
p99.9:42
max:  22625

### syscall
Latency (ns) per iteration
min:  0
avg:  26.15
p50:  41
p90:  42
p99:  42
p99.9:42
max:  10250

### pagefault
Latency (ns) per iteration
min:  792
avg:  2129.56
p50:  1875
p90:  3292
p99:  6417
p99.9:21375
max:  311125


