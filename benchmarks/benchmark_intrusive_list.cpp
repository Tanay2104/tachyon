#include <benchmark/benchmark.h>

#include <algorithm>
#include <deque>
#include <list>
#include <memory>
#include <random>
#include <vector>

// Suppress pedantic warnings for the GCC/Clang statement-expression ({...})
// used in the header's container_of macro.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include "containers/intrusive_list.hpp"
#pragma GCC diagnostic pop

// -----------------------------------------------------------------------------
// Data Structures
// -----------------------------------------------------------------------------

// A struct simulating a typical order/message in a matching engine (20-60
// bytes). Padding is added to ensure we aren't just benchmarking strict L1 hits
// if the structs were too small.
struct BenchData {
  int id;
  long padding[4];  // Padding to make the struct size realistic (~48 bytes +
                    // hook)
  IntrusiveListNode intr_node;  // The hook for your list

  BenchData(int i) : id(i) {
    // Initialize padding to avoid dirty memory affecting results
    for (auto& p : padding) p = 0;
  }
};

// -----------------------------------------------------------------------------
// Fixture 1: Contiguous Memory (Standard Benchmarks)
// -----------------------------------------------------------------------------
// This tests the "best case" scenario where nodes are allocated in a pool
// (std::vector), which is common in high-performance engines.

class ContainerBench : public benchmark::Fixture {
 protected:
  std::vector<BenchData> object_pool;

  void SetUp(const benchmark::State& state) override {
    // Reserve memory to prevent reallocation during setup
    long size = state.range(0);
    object_pool.reserve(size);
    for (int i = 0; i < size; ++i) {
      object_pool.emplace_back(i);
    }
  }

  void TearDown(const benchmark::State&) override { object_pool.clear(); }
};

// -----------------------------------------------------------------------------
// Benchmark Group 1: Push Back (Latency & Allocation)
// -----------------------------------------------------------------------------

BENCHMARK_DEFINE_F(ContainerBench,
                   Intrusive_PushBack)(benchmark::State& state) {
  for (auto _ : state) {
    state.PauseTiming();
    intrusive_list<BenchData> list;
    state.ResumeTiming();

    for (auto& item : object_pool) {
      list.push_back(item);
    }

    benchmark::DoNotOptimize(list.size());
  }
}

BENCHMARK_DEFINE_F(ContainerBench, StdList_PushBack)(benchmark::State& state) {
  for (auto _ : state) {
    state.PauseTiming();
    std::list<BenchData> list;
    state.ResumeTiming();

    for (const auto& item : object_pool) {
      list.push_back(item);  // Performs heap allocation
    }
    benchmark::DoNotOptimize(list.size());
  }
}

BENCHMARK_DEFINE_F(ContainerBench, StdDeque_PushBack)(benchmark::State& state) {
  for (auto _ : state) {
    state.PauseTiming();
    std::deque<BenchData> list;
    state.ResumeTiming();

    for (const auto& item : object_pool) {
      list.push_back(item);  // Block allocation logic
    }
    benchmark::DoNotOptimize(list.size());
  }
}

// -----------------------------------------------------------------------------
// Benchmark Group 2: Traversal (Cache Locality & Overhead)
// -----------------------------------------------------------------------------

BENCHMARK_DEFINE_F(ContainerBench,
                   Intrusive_Traverse)(benchmark::State& state) {
  intrusive_list<BenchData> list;
  for (auto& item : object_pool) list.push_back(item);

  for (auto _ : state) {
    if (list.size() == 0) continue;

    // Manual Traversal Logic (Simulating lack of iterators)
    // 1. Get the first node from the head data
    IntrusiveListNode* curr_node = &list.front().intr_node;

    while (curr_node != &list.root) {
      // 2. Resolve data from node
      BenchData* data = list.getData(curr_node);

      // 3. Simulate Access
      benchmark::DoNotOptimize(data->id);

      // 4. Move Next
      curr_node = curr_node->prev;
    }
  }
}

BENCHMARK_DEFINE_F(ContainerBench, StdList_Traverse)(benchmark::State& state) {
  std::list<BenchData> list;
  for (const auto& item : object_pool) list.push_back(item);

  for (auto _ : state) {
    for (const auto& item : list) {
      benchmark::DoNotOptimize(item.id);
    }
  }
}

BENCHMARK_DEFINE_F(ContainerBench, StdDeque_Traverse)(benchmark::State& state) {
  std::deque<BenchData> list;
  for (const auto& item : object_pool) list.push_back(item);

  for (auto _ : state) {
    for (const auto& item : list) {
      benchmark::DoNotOptimize(item.id);
    }
  }
}

// -----------------------------------------------------------------------------
// Benchmark Group 3: The FIFO Queue Cycle (Pop Front -> Push Back)
// -----------------------------------------------------------------------------

BENCHMARK_DEFINE_F(ContainerBench, Intrusive_Cycle)(benchmark::State& state) {
  intrusive_list<BenchData> list;
  for (auto& item : object_pool) list.push_back(item);

  for (auto _ : state) {
    if (list.size() > 0) {
      // Intrusive unlink/relink is purely pointer math
      BenchData& d = list.front();
      list.remove(d);
      list.push_back(d);
      benchmark::DoNotOptimize(d.id);
    }
  }
}

BENCHMARK_DEFINE_F(ContainerBench, StdList_Cycle)(benchmark::State& state) {
  std::list<BenchData> list;
  for (const auto& item : object_pool) list.push_back(item);

  for (auto _ : state) {
    if (!list.empty()) {
      // Expensive: Deallocate front node, Allocate new back node
      auto val = list.front();
      list.pop_front();
      list.push_back(val);
      benchmark::DoNotOptimize(val.id);
    }
  }
}

BENCHMARK_DEFINE_F(ContainerBench, StdDeque_Cycle)(benchmark::State& state) {
  std::deque<BenchData> list;
  for (const auto& item : object_pool) list.push_back(item);

  for (auto _ : state) {
    if (!list.empty()) {
      auto val = list.front();
      list.pop_front();
      list.push_back(val);
      benchmark::DoNotOptimize(val.id);
    }
  }
}

// -----------------------------------------------------------------------------
// Fixture 2: Fragmented Memory (Prefetch Optimization Test)
// -----------------------------------------------------------------------------
// This tests the "worst case" (real world) scenario where objects are allocated
// randomly on the heap, causing cache misses during traversal.

class FragmentedBench : public benchmark::Fixture {
 protected:
  std::vector<BenchData*> pointer_pool;
  std::vector<std::unique_ptr<BenchData>> actual_memory;

  void SetUp(const benchmark::State& state) override {
    long size = state.range(0);
    // 1. Allocate nodes individually to scatter them in heap memory
    for (int i = 0; i < size; ++i) {
      actual_memory.push_back(std::make_unique<BenchData>(i));
      pointer_pool.push_back(actual_memory.back().get());
    }

    // 2. Shuffle pointers so the linked list jumps around RAM randomly
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(pointer_pool.begin(), pointer_pool.end(), g);
  }

  void TearDown(const benchmark::State&) override {
    pointer_pool.clear();
    actual_memory.clear();
  }
};

// Case A: Standard Iteration (Stalls on Memory Latency)
BENCHMARK_DEFINE_F(FragmentedBench, NoPrefetch_Walk)(benchmark::State& state) {
  intrusive_list<BenchData> list;
  for (auto* item : pointer_pool) list.push_back(*item);

  for (auto _ : state) {
    if (list.size() == 0) continue;

    // Manual iteration logic without prefetch
    IntrusiveListNode* curr_node = &list.front().intr_node;

    while (curr_node != &list.root) {
      // Fetch data (Cache Miss happens here)
      BenchData* data = list.getData(curr_node);

      // Do work
      benchmark::DoNotOptimize(data->id + 1);

      // Move next (No prefetch hint)
      curr_node = curr_node->prev;
    }
  }
}

// Case B: Prefetch Iteration (Hides Memory Latency)
BENCHMARK_DEFINE_F(FragmentedBench, Prefetch_Walk)(benchmark::State& state) {
  intrusive_list<BenchData> list;
  for (auto* item : pointer_pool) list.push_back(*item);

  for (auto _ : state) {
    // Use the internal for_each which contains __builtin_prefetch
    list.for_each(
        [](BenchData& data) { benchmark::DoNotOptimize(data.id + 1); });
  }
}

// -----------------------------------------------------------------------------
// Registration
// -----------------------------------------------------------------------------

// Run with various sizes.
// 100: L1 Cache fit.
// 4096: L2/L3 boundary.
// 10000: Main Memory pressure.
#define REGISTER_STD(NAME)                   \
  BENCHMARK_REGISTER_F(ContainerBench, NAME) \
      ->Range(100, 10000)                    \
      ->Unit(benchmark::kMicrosecond);

REGISTER_STD(Intrusive_PushBack);
REGISTER_STD(StdList_PushBack);
REGISTER_STD(StdDeque_PushBack);

REGISTER_STD(Intrusive_Traverse);
REGISTER_STD(StdList_Traverse);
REGISTER_STD(StdDeque_Traverse);

REGISTER_STD(Intrusive_Cycle);
REGISTER_STD(StdList_Cycle);
REGISTER_STD(StdDeque_Cycle);

#define REGISTER_FRAG(NAME)                   \
  BENCHMARK_REGISTER_F(FragmentedBench, NAME) \
      ->Range(100, 10000)                     \
      ->Unit(benchmark::kMicrosecond);

REGISTER_FRAG(NoPrefetch_Walk);
REGISTER_FRAG(Prefetch_Walk);

BENCHMARK_MAIN();
