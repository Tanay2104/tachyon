#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <containers/sharded_queue.hpp>
#include <thread>
#include <unordered_set>
#include <vector>

#include "containers/sharded_queue.hpp"

using threadsafe::sharded_queue;

// --------------------------------------------
// Helper utilities
// --------------------------------------------

static void spin_wait_until(std::atomic<bool>& flag) {
  while (!flag.load(std::memory_order_acquire)) {
    std::this_thread::yield();
  }
}

// --------------------------------------------
// 1. Single-thread sanity
// --------------------------------------------

TEST(ShardedQueue_SingleThread, FIFOBehavior) {
  sharded_queue<int> q;

  for (int i = 0; i < 1000; ++i) {
    q.push(i);
  }

  for (int i = 0; i < 1000; ++i) {
    int v;
    ASSERT_TRUE(q.try_pop(v));
    EXPECT_EQ(v, i);
  }

  int dummy;
  EXPECT_FALSE(q.try_pop(dummy));
}

TEST(ShardedQueue_SingleThread, NoDuplicatePops) {
  sharded_queue<int> q;
  std::unordered_set<int> seen;

  for (int i = 0; i < 1000; ++i) q.push(i);

  for (int i = 0; i < 1000; ++i) {
    int v;
    ASSERT_TRUE(q.try_pop(v));
    EXPECT_TRUE(seen.insert(v).second);
  }
}

// --------------------------------------------
// 2. Multi-producer basic correctness
// --------------------------------------------

TEST(ShardedQueue_MPMC, AllElementsEventuallyPopped) {
  sharded_queue<int> q;

  constexpr int producers = 4;
  constexpr int items_per_producer = 10'000;

  std::atomic<int> produced{0};
  std::atomic<int> consumed{0};

  std::vector<std::thread> threads;

  for (int p = 0; p < producers; ++p) {
    threads.emplace_back([&, p] {
      for (int i = 0; i < items_per_producer; ++i) {
        q.push(p * items_per_producer + i);
        produced.fetch_add(1, std::memory_order_relaxed);
      }
    });
  }

  for (auto& t : threads) t.join();

  std::unordered_set<int> seen;
  int value;

  while (consumed.load() < produced.load()) {
    if (q.try_pop(value)) {
      EXPECT_TRUE(seen.insert(value).second);
      consumed.fetch_add(1);
    }
  }

  EXPECT_EQ(seen.size(), producers * items_per_producer);
}

// --------------------------------------------
// 3. Global ordering guarantees
// --------------------------------------------
// This test is EXPECTED TO FAIL intermittently
// --------------------------------------------

TEST(ShardedQueue_MPMC, GlobalInsertionOrder) {
  sharded_queue<uint64_t> q;

  constexpr int producers = 4;
  constexpr int items_per_producer = 2000;

  std::atomic<uint64_t> global_counter{0};
  std::atomic<bool> start{false};

  std::vector<std::thread> threads;

  for (int p = 0; p < producers; ++p) {
    threads.emplace_back([&] {
      spin_wait_until(start);
      for (int i = 0; i < items_per_producer; ++i) {
        uint64_t v = global_counter.fetch_add(1);
        q.push(v);
      }
    });
  }

  start.store(true);

  std::vector<uint64_t> popped;
  popped.reserve(producers * items_per_producer);

  while (popped.size() < producers * items_per_producer) {
    uint64_t v;
    if (q.try_pop(v)) {
      popped.push_back(v);
    }
  }

  for (size_t i = 1; i < popped.size(); ++i) {
    EXPECT_LT(popped[i - 1], popped[i])
        << "Global ordering violated at index " << i;
  }

  for (auto& t : threads) t.join();
}

// --------------------------------------------
// 4. Progress / liveness
// --------------------------------------------

TEST(ShardedQueue_MPMC, NoDeadlockUnderContention) {
  sharded_queue<int> q;

  constexpr int producers = 8;
  constexpr int items_per_producer = 5000;

  std::atomic<bool> done{false};

  std::vector<std::thread> threads;
  for (int p = 0; p < producers; ++p) {
    threads.emplace_back([&] {
      for (int i = 0; i < items_per_producer; ++i) {
        q.push(i);
      }
    });
  }

  std::thread consumer([&] {
    int v;
    int count = 0;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);

    while (count < producers * items_per_producer &&
           std::chrono::steady_clock::now() < deadline) {
      if (q.try_pop(v)) {
        ++count;
      }
    }
    EXPECT_EQ(count, producers * items_per_producer);
    done.store(true);
  });

  for (auto& t : threads) t.join();
  consumer.join();

  EXPECT_TRUE(done.load());
}

// --------------------------------------------
// 5. Race amplification / UB detection
// --------------------------------------------
// Run under TSAN or ASAN
// --------------------------------------------

TEST(ShardedQueue_Stress, HammerTest) {
  sharded_queue<uint64_t> q;

  constexpr int producers = 16;
  constexpr int ops_per_thread = 50'000;

  std::atomic<bool> start{false};
  std::vector<std::thread> threads;

  for (int p = 0; p < producers; ++p) {
    threads.emplace_back([&] {
      spin_wait_until(start);
      for (int i = 0; i < ops_per_thread; ++i) {
        q.push(static_cast<uint64_t>(i));
      }
    });
  }

  start.store(true);

  std::thread consumer([&] {
    uint64_t v;
    size_t pops = 0;
    while (pops < producers * ops_per_thread) {
      if (q.try_pop(v)) {
        ++pops;
      }
    }
  });

  for (auto& t : threads) t.join();
  consumer.join();
}
