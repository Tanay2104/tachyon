#include <benchmark/benchmark.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>
#include <random>
#include <thread>
#include <vector>

#include "containers/lock_queue.hpp"
#include "engine/orderbook.hpp"
#include "engine/types.hpp"
// ----------------------------------------------------------------------------
// BENCHMARK: Single Thread Throughput (Baseline)
// ----------------------------------------------------------------------------
static void BM_LockSTLQueue_Throughput(benchmark::State& state) {
  threadsafe::stl_queue<int> q;
  const int BATCH = 1000;

  for (auto _ : state) {
    for (int i = 0; i < BATCH; ++i) {
      q.push(i);
    }
    for (int i = 0; i < BATCH; ++i) {
      int val;
      q.try_pop(val);
    }
  }
  state.SetItemsProcessed(state.iterations() * BATCH * 2);
}
BENCHMARK(BM_LockSTLQueue_Throughput);

// ----------------------------------------------------------------------------
// BENCHMARK: Multi-Thread Contention (Producer vs Consumer)
// ----------------------------------------------------------------------------
static void BM_LockSTLQueue_Contention(benchmark::State& state) {
  // 1. Data Setup
  threadsafe::stl_queue<int> q;
  const int ITEMS = state.range(0);

  // 2. Synchronization Primitives
  // We use a countdown latch logic.
  // Producer sets it, Consumer decrements it.
  std::atomic<long> items_remaining{0};
  std::atomic<bool> thread_exit{false};

  // 3. Consumer Thread (Kept alive)
  std::thread consumer([&]() {
    int val;
    while (!thread_exit.load(std::memory_order_relaxed)) {
      // Optimization: If work is done, back off to reduce contention
      // during the Producer's setup/teardown phase.
      if (items_remaining.load(std::memory_order_relaxed) <= 0) {
        std::this_thread::yield();
        continue;
      }

      // Attempt pop
      if (q.try_pop(val)) {
        // Success: Decrement counter
        // We use release to ensure the main thread sees this update
        items_remaining.fetch_sub(1, std::memory_order_release);
      } else {
        // Failure: Queue momentarily empty or locked
        std::this_thread::yield();
      }
    }
  });

  // 4. Producer Loop (Main Benchmark)
  for (auto _ : state) {
    state.PauseTiming();

    // A. Sanity Check: Ensure queue is truly empty and counter is 0
    // (This handles any rare race where consumer was slow to update)
    while (items_remaining.load(std::memory_order_acquire) > 0) {
      std::this_thread::yield();
    }
    // Drain any debris if logic drifted (sanity)
    int dump;
    while (q.try_pop(dump));

    // B. Set the goal
    items_remaining.store(ITEMS, std::memory_order_release);

    state.ResumeTiming();

    // C. Produce (Blast data into queue)
    for (int i = 0; i < ITEMS; ++i) {
      q.push(i);
    }

    // D. Wait for Consumer to finish this batch
    // We simply wait until the counter hits 0.
    while (items_remaining.load(std::memory_order_acquire) > 0) {
      std::this_thread::yield();
    }
  }

  // 5. Cleanup
  thread_exit.store(true, std::memory_order_release);
  if (consumer.joinable()) consumer.join();

  // 6. Report Items (Push + Pop count)
  state.SetItemsProcessed(state.iterations() * ITEMS * 2);
}

// Register Benchmark
// Range: Test with 1000, and 100000 items per batch
BENCHMARK(BM_LockSTLQueue_Contention)->Arg(1000)->Arg(100000)->UseRealTime();

// ----------------------------------------------------------------------------
// BENCHMARK: Single Thread Throughput (Baseline)
// ----------------------------------------------------------------------------
static void BM_LockQueue_Throughput(benchmark::State& state) {
  threadsafe::lock_queue<int> q;
  const int BATCH = 1000;

  for (auto _ : state) {
    for (int i = 0; i < BATCH; ++i) {
      q.push(i);
    }
    for (int i = 0; i < BATCH; ++i) {
      int val;
      q.try_pop(val);
    }
  }
  state.SetItemsProcessed(state.iterations() * BATCH * 2);
}
BENCHMARK(BM_LockQueue_Throughput);

// ----------------------------------------------------------------------------
// BENCHMARK: Multi-Thread Contention (Producer vs Consumer)
// ----------------------------------------------------------------------------
static void BM_LockQueue_Contention(benchmark::State& state) {
  // 1. Data Setup
  threadsafe::lock_queue<int> q;
  const int ITEMS = state.range(0);

  // 2. Synchronization Primitives
  // We use a countdown latch logic.
  // Producer sets it, Consumer decrements it.
  std::atomic<long> items_remaining{0};
  std::atomic<bool> thread_exit{false};

  // 3. Consumer Thread (Kept alive)
  std::thread consumer([&]() {
    int val;
    while (!thread_exit.load(std::memory_order_relaxed)) {
      // Optimization: If work is done, back off to reduce contention
      // during the Producer's setup/teardown phase.
      if (items_remaining.load(std::memory_order_relaxed) <= 0) {
        std::this_thread::yield();
        continue;
      }

      // Attempt pop
      if (q.try_pop(val)) {
        // Success: Decrement counter
        // We use release to ensure the main thread sees this update
        items_remaining.fetch_sub(1, std::memory_order_release);
      } else {
        // Failure: Queue momentarily empty or locked
        std::this_thread::yield();
      }
    }
  });

  // 4. Producer Loop (Main Benchmark)
  for (auto _ : state) {
    state.PauseTiming();

    // A. Sanity Check: Ensure queue is truly empty and counter is 0
    // (This handles any rare race where consumer was slow to update)
    while (items_remaining.load(std::memory_order_acquire) > 0) {
      std::this_thread::yield();
    }
    // Drain any debris if logic drifted (sanity)
    int dump;
    while (q.try_pop(dump));

    // B. Set the goal
    items_remaining.store(ITEMS, std::memory_order_release);

    state.ResumeTiming();

    // C. Produce (Blast data into queue)
    for (int i = 0; i < ITEMS; ++i) {
      q.push(i);
    }

    // D. Wait for Consumer to finish this batch
    // We simply wait until the counter hits 0.
    while (items_remaining.load(std::memory_order_acquire) > 0) {
      std::this_thread::yield();
    }
  }

  // 5. Cleanup
  thread_exit.store(true, std::memory_order_release);
  if (consumer.joinable()) consumer.join();

  // 6. Report Items (Push + Pop count)
  state.SetItemsProcessed(state.iterations() * ITEMS * 2);
}

// Register Benchmark
// Range: Test with 1000, and 100000 items per batch
BENCHMARK(BM_LockQueue_Contention)->Arg(1000)->Arg(100000)->UseRealTime();

// BENCHMARK_MAIN();
