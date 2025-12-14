#include <gtest/gtest.h>

#include <cstdint>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "containers/flat_hashmap.hpp"

namespace {

struct Value32 {
  uint64_t a, b, c, d;
  bool operator==(const Value32& o) const {
    return a == o.a && b == o.b && c == o.c && d == o.d;
  }
};

struct Value48 {
  uint64_t a, b, c, d, e, f;
  bool operator==(const Value48& o) const {
    return a == o.a && b == o.b && c == o.c && d == o.d && e == o.e && f == o.f;
  }
};

}  // namespace

// ------------------------------------------------------------
// Basic correctness
// ------------------------------------------------------------

TEST(FlatHashMapBasic, InsertAndGetSingle) {
  flat_hashmap<uint64_t, std::string> map(8);
  map.insert(1, "one");
  EXPECT_EQ(map.get(1), "one");
}

TEST(FlatHashMapBasic, OverwriteExistingKey) {
  flat_hashmap<uint64_t, int> map(8);
  map.insert(5, 10);
  map.insert(5, 20);
  EXPECT_EQ(map.get(5), 20);
}

TEST(FlatHashMapBasic, GetNonExistentThrows) {
  flat_hashmap<int, int> map(8);
  EXPECT_THROW(map.get(42), std::out_of_range);
}

// ------------------------------------------------------------
// Collision and probing behavior
// ------------------------------------------------------------

TEST(FlatHashMapProbing, HandlesLinearProbingCollisions) {
  flat_hashmap<int, int> map(4);

  // Force collisions via small table
  map.insert(1, 100);
  map.insert(5, 200);  // likely same bucket
  map.insert(9, 300);

  EXPECT_EQ(map.get(1), 100);
  EXPECT_EQ(map.get(5), 200);
  EXPECT_EQ(map.get(9), 300);
}

// ------------------------------------------------------------
// Tombstones
// ------------------------------------------------------------

TEST(FlatHashMapTombstone, RemoveAndReuseSlot) {
  flat_hashmap<int, int> map(8);

  map.insert(1, 10);
  map.insert(9, 90);  // collision with 1 likely

  map.remove(1);

  // Slot should be reusable
  map.insert(17, 170);

  EXPECT_EQ(map.get(9), 90);
  EXPECT_EQ(map.get(17), 170);
}

TEST(FlatHashMapTombstone, RemoveNonExistentThrows) {
  flat_hashmap<int, int> map(8);
  EXPECT_THROW(map.remove(123), std::out_of_range);
}

// ------------------------------------------------------------
// Rehashing / growth
// ------------------------------------------------------------

TEST(FlatHashMapGrowth, GrowsAndPreservesAllElements) {
  flat_hashmap<int, int> map(4);

  for (int i = 0; i < 1000; ++i) {
    map.insert(i, i * 10);
  }

  for (int i = 0; i < 1000; ++i) {
    EXPECT_EQ(map.get(i), i * 10);
  }
}

TEST(FlatHashMapGrowth, GrowthPreservesAfterRemovals) {
  flat_hashmap<int, int> map(4);

  for (int i = 0; i < 100; ++i) {
    map.insert(i, i);
  }

  for (int i = 0; i < 50; ++i) {
    map.remove(i);
  }

  for (int i = 100; i < 300; ++i) {
    map.insert(i, i * 2);
  }

  for (int i = 50; i < 300; ++i) {
    EXPECT_EQ(map.get(i), i < 100 ? i : i * 2);
  }
}

// ------------------------------------------------------------
// Realistic payload sizes
// ------------------------------------------------------------

TEST(FlatHashMapPayload, Value32Bytes) {
  flat_hashmap<uint64_t, Value32> map(8);

  Value32 v{1, 2, 3, 4};
  map.insert(42, v);

  EXPECT_EQ(map.get(42), v);
}

TEST(FlatHashMapPayload, Value48Bytes) {
  flat_hashmap<uint64_t, Value48> map(8);

  Value48 v{1, 2, 3, 4, 5, 6};
  map.insert(99, v);

  EXPECT_EQ(map.get(99), v);
}

// ------------------------------------------------------------
// Stress + randomized
// ------------------------------------------------------------

TEST(FlatHashMapStress, RandomInsertGet) {
  flat_hashmap<uint64_t, uint64_t> map(16);
  std::mt19937_64 rng(123);

  constexpr int N = 10'000;
  std::vector<uint64_t> keys;

  for (int i = 0; i < N; ++i) {
    uint64_t k = rng();
    keys.push_back(k);
    map.insert(k, i);
  }

  for (int i = 0; i < N; ++i) {
    EXPECT_EQ(map.get(keys[i]), static_cast<uint64_t>(i));
  }
}
