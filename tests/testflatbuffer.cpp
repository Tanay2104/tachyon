#include "containers/flat_buffer.hpp" // Assume your header is named this
#include <gtest/gtest.h>

class FlatBufferTest : public ::testing::Test {
protected:
  flat_buffer<char> buffer{100}; // Start with small capacity for testing
};

// 1. Basic Lifecycle
TEST_F(FlatBufferTest, InitialState) {
  EXPECT_EQ(buffer.size(), 0);
  EXPECT_NE(buffer.begin(), nullptr);
}

// 2. Insertion and Data Integrity
TEST_F(FlatBufferTest, InsertAndRead) {
  const char *msg = "Hello World";
  size_t len = strlen(msg);

  buffer.insert(const_cast<char *>(msg), len);

  EXPECT_EQ(buffer.size(), len);
  EXPECT_EQ(std::memcmp(buffer.begin(), msg, len), 0);
}

// 3. Erasure (Head movement)
TEST_F(FlatBufferTest, EraseData) {
  buffer.insert(const_cast<char *>("ABCDE"), 5);
  buffer.erase(2); // Remove "AB"

  EXPECT_EQ(buffer.size(), 3);
  EXPECT_EQ(*buffer.begin(), 'C');
}

// 4. Triggering Reset (Saturation)
TEST_F(FlatBufferTest, ResetTrigger) {
  // Fill buffer partially, then erase to create a gap at the head
  char data[40];
  buffer.insert(data, 40);
  buffer.erase(30);
  // head is 30, tail is 40, size is 10.

  // Attempt to insert more than remaining space at tail
  // This should trigger reset() because head (30) > capacity - 1 - size
  // (100-1-10=89) is false... Wait, your reset logic: if (head >= capacity - 1
  // - size()) Let's force it:
  flat_buffer<char> small_buf{10};
  small_buf.insert(const_cast<char *>("012345"), 6); // tail=6
  small_buf.erase(4);                                // head=4, size=2

  // Now head(4) >= capacity(10) - 1 - size(2) = 7 (False)
  // Let's add more until head is large.
  small_buf.insert(const_cast<char *>("678"), 3); // tail=9, size=5, head=4
  // head(4) >= 10 - 1 - 5 = 4 (True!) -> Triggers reset

  small_buf.insert(const_cast<char *>("9"), 1);

  EXPECT_EQ(small_buf.size(), 6);
  EXPECT_EQ(std::memcmp(small_buf.begin(), "456789", 6), 0);
}

// 5. Dynamic Growth
TEST_F(FlatBufferTest, AutomaticGrowth) {
  std::string large_data(200, 'z');
  // Default capacity was 100, this MUST trigger grow()
  buffer.insert(large_data.data(), large_data.size());

  EXPECT_GT(buffer.size(), 100);
  EXPECT_EQ(buffer.size(), 200);
  EXPECT_EQ(buffer.begin()[199], 'z');
}

// 6. Clear logic
TEST_F(FlatBufferTest, ClearResetsPointers) {
  buffer.insert(const_cast<char *>("test"), 4);
  buffer.clear();
  EXPECT_EQ(buffer.size(), 0);
  EXPECT_EQ(buffer.begin(), buffer.end());
}
