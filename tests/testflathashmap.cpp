#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "containers/flat_hashmap.hpp"

/*
TESTING PHILOSOPHY

1. Correctness under normal usage
2. Linear probing correctness (collisions)
3. Overwrite semantics
4. Tombstone behavior
5. Exception guarantees
6. Boundary conditions (full table)
7. Reference stability for get()
*/

/* -------------------------------------------------------------
 * BASIC INSERT / GET
 * ------------------------------------------------------------- */

TEST(FlatHashMapBasic, InsertAndGetSingle) {
  flat_hashmap<int, int> map(8);

  int k = 42;
  int v = 100;

  EXPECT_TRUE(map.insert(k, v));
  EXPECT_NO_THROW({
    int& ref = map.get(k);
    EXPECT_EQ(ref, v);
  });
}

TEST(FlatHashMapBasic, InsertMultipleDistinctKeys) {
  flat_hashmap<int, std::string> map(16);

  for (int i = 0; i < 10; ++i) {
    int key = i;
    std::string value = "val" + std::to_string(i);
    EXPECT_TRUE(map.insert(key, value));
  }

  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(map.get(i), "val" + std::to_string(i));
  }
}

/* -------------------------------------------------------------
 * OVERWRITE SEMANTICS
 * ------------------------------------------------------------- */

TEST(FlatHashMapBasic, InsertSameKeyOverwritesValue) {
  flat_hashmap<int, int> map(8);

  int k = 1;
  int v1 = 10;
  int v2 = 20;

  EXPECT_TRUE(map.insert(k, v1));
  EXPECT_EQ(map.get(k), v1);

  EXPECT_TRUE(map.insert(k, v2));
  EXPECT_EQ(map.get(k), v2);
}

/* -------------------------------------------------------------
 * REFERENCE SEMANTICS
 * ------------------------------------------------------------- */

TEST(FlatHashMapBasic, GetReturnsMutableReference) {
  flat_hashmap<int, int> map(8);

  int k = 5;
  int v = 50;

  map.insert(k, v);

  int& ref = map.get(k);
  ref = 99;

  EXPECT_EQ(map.get(k), 99);
}

/* -------------------------------------------------------------
 * EXCEPTION BEHAVIOR
 * ------------------------------------------------------------- */

TEST(FlatHashMapExceptions, GetMissingKeyThrows) {
  flat_hashmap<int, int> map(8);

  EXPECT_THROW(map.get(123), std::out_of_range);
}

TEST(FlatHashMapExceptions, RemoveMissingKeyThrows) {
  flat_hashmap<int, int> map(8);

  EXPECT_THROW(map.remove(999), std::out_of_range);
}

/* -------------------------------------------------------------
 * COLLISION & LINEAR PROBING
 * ------------------------------------------------------------- */

/*
We deliberately create collisions by using a tiny table.
This verifies linear probing correctness.
*/
TEST(FlatHashMapProbing, HandlesCollisionsCorrectly) {
  flat_hashmap<int, int> map(4);

  int k1 = 1;
  int k2 = 5;  // Likely collides with k1 modulo small N
  int k3 = 9;  // Another collision

  int v1 = 10;
  int v2 = 20;
  int v3 = 30;

  EXPECT_TRUE(map.insert(k1, v1));
  EXPECT_TRUE(map.insert(k2, v2));
  EXPECT_TRUE(map.insert(k3, v3));

  EXPECT_EQ(map.get(k1), v1);
  EXPECT_EQ(map.get(k2), v2);
  EXPECT_EQ(map.get(k3), v3);
}

/* -------------------------------------------------------------
 * TOMBONE BEHAVIOR
 * ------------------------------------------------------------- */

TEST(FlatHashMapTombstone, RemoveThenGetThrows) {
  flat_hashmap<int, int> map(8);

  int k = 7;
  int v = 70;

  map.insert(k, v);
  map.remove(k);

  EXPECT_THROW(map.get(k), std::out_of_range);
}

TEST(FlatHashMapTombstone, RemoveAllowsSlotReuse) {
  flat_hashmap<int, int> map(4);

  int k1 = 1, v1 = 10;
  int k2 = 5, v2 = 20;  // collision
  int k3 = 9, v3 = 30;  // collision

  map.insert(k1, v1);
  map.insert(k2, v2);
  map.remove(k1);

  EXPECT_TRUE(map.insert(k3, v3));
  EXPECT_EQ(map.get(k3), v3);
}

/* -------------------------------------------------------------
 * FULL TABLE BEHAVIOR
 * ------------------------------------------------------------- */

TEST(FlatHashMapCapacity, InsertFailsWhenFull) {
  flat_hashmap<int, int> map(2);

  int k1 = 1, v1 = 10;
  int k2 = 2, v2 = 20;
  int k3 = 3, v3 = 30;

  EXPECT_TRUE(map.insert(k1, v1));
  EXPECT_TRUE(map.insert(k2, v2));

  EXPECT_FALSE(map.insert(k3, v3));
}

/* -------------------------------------------------------------
 * STABILITY & INTEGRITY
 * ------------------------------------------------------------- */

TEST(FlatHashMapStability, MultipleOperationsMaintainIntegrity) {
  flat_hashmap<int, int> map(16);

  for (int i = 0; i < 10; ++i) {
    int key = i;
    int value = i * 100;
    map.insert(key, value);
  }

  for (int i = 0; i < 5; ++i) {
    map.remove(i);
  }

  for (int i = 5; i < 10; ++i) {
    EXPECT_EQ(map.get(i), i * 100);
  }

  for (int i = 0; i < 5; ++i) {
    EXPECT_THROW(map.get(i), std::out_of_range);
  }
}
