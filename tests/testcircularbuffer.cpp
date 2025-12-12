#include <gtest/gtest.h>

#include <cstdio>  // For remove()
#include <string>
#include <vector>

// Include the header file provided
#include "containers/circular_buffer.hpp"

// Test Fixture for Integer Buffer
class CircularBufferTest : public ::testing::Test {
 protected:
  // Create a buffer with Size N=5 (Capacity = 4 due to "one slot wasted")
  circular_buffer<int> cb{5};

  void SetUp() override {
    // Code to run before each test (optional here as we init in constructor)
  }

  void TearDown() override {
    // Code to run after each test
  }
};

// 1. Initialization and Empty State
TEST_F(CircularBufferTest, IsEmptyOnInitialization) {
  EXPECT_TRUE(cb.empty());
  EXPECT_FALSE(cb.full());
  EXPECT_EQ(cb.size(), 0);
}

// 2. Basic Push and Pop
TEST_F(CircularBufferTest, PushAndPopSingleElement) {
  cb.push(42);

  EXPECT_FALSE(cb.empty());
  EXPECT_EQ(cb.size(), 1);

  int val = 0;
  cb.pop(val);

  EXPECT_EQ(val, 42);
  EXPECT_TRUE(cb.empty());
  EXPECT_EQ(cb.size(), 0);
}

// 3. FIFO Order Verification
TEST_F(CircularBufferTest, PreservesFifoOrder) {
  cb.push(1);
  cb.push(2);
  cb.push(3);

  int val;
  cb.pop(val);
  EXPECT_EQ(val, 1);
  cb.pop(val);
  EXPECT_EQ(val, 2);
  cb.pop(val);
  EXPECT_EQ(val, 3);
}

// 4. Fullness and Capacity
TEST_F(CircularBufferTest, ReportsFullCorrectly) {
  // Capacity is N-1. N=5, so Capacity=4.
  cb.push(1);
  cb.push(2);
  cb.push(3);
  EXPECT_FALSE(cb.full());

  cb.push(4);
  EXPECT_TRUE(cb.full());
  EXPECT_EQ(cb.size(), 4);
}

// 5. Circular Overwrite Behavior (Rigorous Logic Test)
// NOTE: This test will likely FAIL with the provided header due to the `head++`
// bug.
TEST_F(CircularBufferTest, OverwritesOldestOnFullPush) {
  // Fill buffer: [1, 2, 3, 4]
  for (int i = 1; i <= 4; ++i) cb.push(i);

  ASSERT_TRUE(cb.full());
  ASSERT_EQ(cb[0], 1);  // Head is at 1

  // Push 5. Should drop 1. Buffer becomes [2, 3, 4, 5]
  cb.push(5);

  EXPECT_TRUE(cb.full());   // Should still be full
  EXPECT_EQ(cb.size(), 4);  // Size should remain max

  // Check elements via operator[]
  // Index 0 should now be the new head (2)
  EXPECT_EQ(cb[0], 2);
  EXPECT_EQ(cb[3], 5);

  // Verify via pop
  int val;
  cb.pop(val);
  EXPECT_EQ(val, 2);  // 1 was overwritten
}

// 6. Wrap Around Logic (The "Circular" aspect)
// NOTE: This checks if the buffer handles multiple loops correctly.
TEST_F(CircularBufferTest, HandlesMultipleWraps) {
  // Push and Pop 100 items through a buffer of size 5
  for (int i = 0; i < 100; ++i) {
    cb.push(i);
    if (cb.size() == 4) {
      int val;
      cb.pop(val);
      // Verify we are popping sequential numbers
      EXPECT_EQ(val, i - 3);
    }
  }
  EXPECT_FALSE(cb.empty());
}

// 7. Random Access Operator Bounds
TEST_F(CircularBufferTest, OperatorBracketAccess) {
  cb.push(10);
  cb.push(20);

  EXPECT_EQ(cb[0], 10);
  EXPECT_EQ(cb[1], 20);
}

// 8. Exception Handling
TEST_F(CircularBufferTest, ThrowsOutOfRange) {
  cb.push(1);

  // Accessing index >= size() should throw
  EXPECT_THROW(cb[1],
               std::out_of_range);  // Index 1 is out of bounds (size is 1)
  EXPECT_THROW(cb[5], std::out_of_range);

  // Empty buffer access
  cb.clear();
  EXPECT_THROW(cb[0], std::out_of_range);
}

// 9. Clear Functionality
TEST_F(CircularBufferTest, ClearResetsBuffer) {
  cb.push(1);
  cb.push(2);
  cb.clear();

  EXPECT_TRUE(cb.empty());
  EXPECT_EQ(cb.size(), 0);
  EXPECT_FALSE(cb.full());

  // Verify we can start over
  cb.push(10);
  EXPECT_EQ(cb.size(), 1);
}

// 10. Type Safety (Complex Types)
TEST(CircularBufferComplexTest, HandlesStrings) {
  circular_buffer<std::string> strBuf(3);  // Cap 2

  std::string s1 = "Hello";
  std::string s2 = "World";

  strBuf.push(s1);
  strBuf.push(s2);

  EXPECT_EQ(strBuf[0], "Hello");
  EXPECT_EQ(strBuf[1], "World");

  std::string out;
  strBuf.pop(out);
  EXPECT_EQ(out, "Hello");
}

/*
   11. Dump to File
*/

TEST(CircularBufferIOTest, DumpsToFile) {
  circular_buffer<int> cb(5);
  cb.push(100);
  cb.push(200);

  std::string filename = "test_dump.txt";

  // Lambda to write format
  auto dumper = [](int& val, std::ofstream& out) { out << val << " "; };

  cb.dump(filename, dumper, std::ios::trunc);

  // Verify file content
  std::ifstream inFile(filename);
  int val;
  std::vector<int> readValues;
  while (inFile >> val) {
    readValues.push_back(val);
  }
  inFile.close();
  std::remove(filename.c_str());  // Cleanup

  ASSERT_EQ(readValues.size(), 2);
  EXPECT_EQ(readValues[0], 100);
  EXPECT_EQ(readValues[1], 200);
}

TEST_F(CircularBufferTest, DeepCopyConstructor) {
  // 1. Setup original buffer
  cb.push(10);
  cb.push(20);
  cb.push(30);

  // 2. Create a copy
  circular_buffer<int> copy_cb = cb;

  // 3. Verify content matches
  ASSERT_EQ(copy_cb.size(), cb.size());
  EXPECT_EQ(copy_cb[0], 10);
  EXPECT_EQ(copy_cb[1], 20);
  EXPECT_EQ(copy_cb[2], 30);

  // 4. Modify the COPY
  int val;
  copy_cb.pop(val);   // remove 10
  copy_cb.push(999);  // add 999

  // 5. Verify ORIGINAL is untouched (Deep Copy proof)
  EXPECT_EQ(cb.size(), 3);
  EXPECT_EQ(cb[0], 10);  // Original still has 10 at head

  // 6. Verify COPY is changed
  EXPECT_EQ(copy_cb.size(), 3);
  EXPECT_EQ(copy_cb[0], 20);  // Head moved
  EXPECT_EQ(copy_cb[2], 999);
}

TEST_F(CircularBufferTest, DeepCopyAssignment) {
  cb.push(1);
  cb.push(2);

  circular_buffer<int> other_cb(10);  // Different size
  other_cb.push(100);

  // Assignment
  other_cb = cb;

  // Verify state
  EXPECT_EQ(other_cb.size(), 2);
  EXPECT_EQ(other_cb[0], 1);
  EXPECT_EQ(other_cb[1], 2);

  // Modify original
  cb.push(3);

  // Verify assigned copy is independent
  EXPECT_EQ(other_cb.size(), 2);
  EXPECT_THROW(other_cb[2], std::out_of_range);
}

TEST_F(CircularBufferTest, MoveConstructor) {
  cb.push(42);
  cb.push(84);

  // Move cb into moved_cb
  circular_buffer<int> moved_cb = std::move(cb);

  // Verify new owner
  EXPECT_EQ(moved_cb.size(), 2);
  EXPECT_EQ(moved_cb[0], 42);
  EXPECT_EQ(moved_cb[1], 84);

  // Verify old owner is empty/reset
  // Note: Calling methods on a moved-from object is valid but result depends on
  // implementation. Our impl sets N=0, so size() should be 0.
  EXPECT_EQ(cb.size(), 0);
  EXPECT_TRUE(cb.empty());

  // Pushing to moved-from object (should not crash, might throw or do nothing
  // depending on logic) Our logic: N is 0. push() -> full() -> checks
  // head/tail/N. It is safe, but functionally dead until reassigned or
  // effectively cleared.
}

TEST_F(CircularBufferTest, MoveAssignment) {
  cb.push(10);

  circular_buffer<int> target(5);
  target.push(99);  // Garbage data to be overwritten

  target = std::move(cb);

  // Target has data
  EXPECT_EQ(target.size(), 1);
  EXPECT_EQ(target[0], 10);

  // Source is empty
  EXPECT_EQ(cb.size(), 0);
}

TEST_F(CircularBufferTest, SelfAssignmentSafe) {
  cb.push(1);

  // Copy Self-Assignment
  cb = cb;
  EXPECT_EQ(cb.size(), 1);
  EXPECT_EQ(cb[0], 1);

  // Move Self-Assignment
  cb = std::move(cb);
  EXPECT_EQ(cb.size(), 1);
  EXPECT_EQ(cb[0], 1);
}

// Ensure logic fixes are present
TEST_F(CircularBufferTest, FullBufferHeadUpdatesCorrectly) {
  // Fill buffer [0, 1, 2, 3] (Capacity 4)
  for (int i = 0; i < 4; i++) cb.push(i);

  ASSERT_TRUE(cb.full());
  ASSERT_EQ(cb[0], 0);

  // Overwrite
  cb.push(4);

  // New state should be [1, 2, 3, 4]
  EXPECT_TRUE(cb.full());
  EXPECT_EQ(cb.size(), 4);
  EXPECT_EQ(cb[0], 1);  // Head moved
  EXPECT_EQ(cb[3], 4);  // Tail updated
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
