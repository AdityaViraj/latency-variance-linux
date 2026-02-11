#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>

using Clock = std::chrono::steady_clock;

// Case A: two atomics likely land on the SAME cache line => false sharing
struct CountersPacked {
  std::atomic<uint64_t> a{0};
  std::atomic<uint64_t> b{0};
};

// Case B: force them onto separate cache lines => reduce false sharing
struct CountersPadded {
  alignas(64) std::atomic<uint64_t> a{0};
  alignas(64) std::atomic<uint64_t> b{0};
};

template <typename Counters>
void run_case(const char *label, int seconds) {
  Counters c;

  std::atomic<bool> start{false};
  std::atomic<bool> stop{false};

  uint64_t itA = 0, itB = 0;

  auto workerA = [&] {
    while (!start.load(std::memory_order_acquire)) {}
    while (!stop.load(std::memory_order_relaxed)) {
      c.a.fetch_add(1, std::memory_order_relaxed);
      itA++;
    }
  };

  auto workerB = [&] {
    while (!start.load(std::memory_order_acquire)) {}
    while (!stop.load(std::memory_order_relaxed)) {
      c.b.fetch_add(1, std::memory_order_relaxed);
      itB++;
    }
  };

  std::thread t1(workerA);
  std::thread t2(workerB);

  std::cout << "\n" << label << "\n";
  auto t0 = Clock::now();
  start.store(true, std::memory_order_release);

  std::this_thread::sleep_for(std::chrono::seconds(seconds));
  stop.store(true, std::memory_order_relaxed);

  t1.join();
  t2.join();
  auto t1t = Clock::now();

  double elapsed = std::chrono::duration<double>(t1t - t0).count();
  uint64_t total = itA + itB;

  std::cout << "  seconds: " << seconds << " (measured " << elapsed << ")\n";
  std::cout << "  threadA iters: " << itA << "\n";
  std::cout << "  threadB iters: " << itB << "\n";
  std::cout << "  total iters:   " << total << "\n";
  std::cout << "  iters/sec:     " << (double)total / elapsed << "\n";
  std::cout << "  final a,b:     " << c.a.load(std::memory_order_relaxed) << ", "
            << c.b.load(std::memory_order_relaxed) << "\n";
}

int main(int argc, char **argv) {
  int seconds = 2;
  if (argc >= 2) seconds = std::max(1, std::atoi(argv[1]));

  std::cout << "False Sharing microbenchmark (2 threads increment adjacent counters)\n";
  std::cout << "Tip: run a few times; results vary due to OS noise / turbo / scheduling.\n";

  run_case<CountersPacked>(
      "CASE A: packed atomics (likely SAME cache line)  => false sharing (more bouncing)",
      seconds);

  run_case<CountersPadded>(
      "CASE B: padded atomics (separate cache lines)    => less false sharing (less bouncing)",
      seconds);

  std::cout << "\nInterpretation:\n";
  std::cout << "  If CASE B shows higher iters/sec than CASE A, you are seeing false sharing.\n";
  std::cout << "  The packed case forces both cores to fight over ownership of one cache line.\n";
  std::cout << "  The padded case reduces cache-line ping-pong.\n";
  return 0;
}
