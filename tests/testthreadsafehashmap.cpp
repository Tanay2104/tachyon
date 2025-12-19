#include "containers/threadsafe_hashmap.hpp"
#include <gtest/gtest.h>
#include <numeric>
#include <thread>
#include <vector>

class HashmapTest : public ::testing::Test {
protected:
  threadsafe::hashmap<int, int *> map{16};
};

// 1. Basic Functional Tests
TEST_F(HashmapTest, InsertAndRetrieve) {
  int val = 42;
  map.insert({1, &val});

  EXPECT_TRUE(map.contains(1));
  EXPECT_EQ(*map.at(1), 42);
}

TEST_F(HashmapTest, EraseExistingKey) {
  int val = 100;
  map.insert({10, &val});
  map.erase(10);
  EXPECT_FALSE(map.contains(10));
}

TEST_F(HashmapTest, AtThrowsOnMissingKey) {
  EXPECT_THROW(map.at(999), std::runtime_error);
}

// 2. Rigorous Concurrency: Multi-threaded Read, Single-threaded Write
TEST_F(HashmapTest, ConcurrentReadSingleWrite) {
  int shared_val = 100;
  map.insert({1, &shared_val});

  std::atomic<bool> running{true};

  // Reader threads
  auto reader = [&]() {
    while (running) {
      if (map.contains(1)) {
        int *ptr = map.at(1);
        EXPECT_NE(ptr, nullptr);
      }
    }
  };

  std::vector<std::thread> threads;
  for (int i = 0; i < 8; ++i)
    threads.emplace_back(reader);

  // Single Writer thread
  for (int i = 0; i < 1000; ++i) {
    map.insert({1, &shared_val});
  }

  running = false;
  for (auto &t : threads)
    t.join();
}

// 3. Stress Test: Multiple Writers on Different Buckets
TEST_F(HashmapTest, ConcurrentInsertions) {
  const int num_threads = 10;
  const int inserts_per_thread = 1000;
  std::vector<std::thread> threads;

  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([this, i, inserts_per_thread]() {
      for (int j = 0; j < inserts_per_thread; ++j) {
        int key = i * inserts_per_thread + j;
        // Using a dummy pointer
        this->map.insert({key, reinterpret_cast<int *>(0x123)});
      }
    });
  }

  for (auto &t : threads)
    t.join();

  // Verify all inserts are present
  for (int i = 0; i < num_threads * inserts_per_thread; ++i) {
    ASSERT_TRUE(map.contains(i)) << "Missing key: " << i;
  }
}

// 4. Collision Handling (Stress one bucket)
TEST_F(HashmapTest, HashCollisionConcurrency) {
  // Force many keys into the same bucket (assuming % 16)
  std::vector<int> colliding_keys = {16, 32, 48, 64, 80, 96};

  auto task = [&]() {
    for (int key : colliding_keys) {
      map.insert({key, NULL});
      map.contains(key);
    }
  };

  std::thread t1(task);
  std::thread t2(task);
  t1.join();
  t2.join();

  for (int key : colliding_keys) {
    EXPECT_TRUE(map.contains(key));
  }
}
