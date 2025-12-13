#include <gtest/gtest.h>

#include "containers/intrusive_list.hpp"  // Assuming your file is named this

// -----------------------------------------------------------------------------
// Test Helpers & Data Structures
// -----------------------------------------------------------------------------

// A sample data structure that satisfies the concept
struct TestData {
  int id;
  std::string name;

  // The required hook.
  // Intentionally not at the start of the struct to test offsetof calculation.
  double padding;
  IntrusiveListNode intr_node;

  TestData(int i, std::string n) : id(i), name(n), padding(0.0) {}

  // Helper to allow comparison in tests
  bool operator==(const TestData& other) const {
    return id == other.id && name == other.name;
  }
};

// A Fixture for clean setups
class IntrusiveListTest : public ::testing::Test {
 protected:
  intrusive_list<TestData> list;

  // Pre-allocated nodes (since intrusive lists don't own memory)
  TestData d1{1, "One"};
  TestData d2{2, "Two"};
  TestData d3{3, "Three"};
};

// -----------------------------------------------------------------------------
// Section 1: Basic Intrusive Mechanics (The "No Copy" verification)
// -----------------------------------------------------------------------------

TEST_F(IntrusiveListTest, IntrusiveProperty_NoCopy) {
  // Action: Push d1 into the list
  list.push_back(d1);
  // Check 1: List size is 1
  EXPECT_EQ(list.size(), 1);

  // Action: Modify d1 EXTERNAL to the list
  d1.name = "Modified";

  // Check 2: Accessing the element through the list reflects the change
  // If the list made a copy, this would still be "One".
  EXPECT_EQ(list.back().name, "Modified");

  // Check 3: Address identity
  // The address of the object in the list should be the address of d1
  EXPECT_EQ(&list.back(), &d1);
}

// -----------------------------------------------------------------------------
// Section 2: Push Back & Tail Logic
// -----------------------------------------------------------------------------

TEST_F(IntrusiveListTest, PushBack_MaintainsOrder) {
  list.push_back(d1);
  list.push_back(d2);
  list.push_back(d3);

  EXPECT_EQ(list.size(), 3);
  // Verify Back
  EXPECT_EQ(list.back().id, 3);

  // Verify Front
  EXPECT_EQ(list.front().id, 1);
}

TEST_F(IntrusiveListTest, PopBack_LIFO) {
  list.push_back(d1);
  list.push_back(d2);

  TestData result{0, ""};

  list.pop_back(result);
  EXPECT_EQ(result.id, 2);  // Should pop d2
  EXPECT_EQ(list.size(), 1);
  EXPECT_EQ(list.back().id, 1);  // d1 is now back

  list.pop_back(result);
  EXPECT_EQ(result.id, 1);  // Should pop d1
  EXPECT_EQ(list.size(), 0);
}

// -----------------------------------------------------------------------------
// Section 3: Push Front & Head Logic
// -----------------------------------------------------------------------------

TEST_F(IntrusiveListTest, PushFront_ReversesOrder) {
  list.push_front(d1);  // List: [1]
  EXPECT_EQ(list.front().id, 1);

  list.push_front(d2);  // List: [2, 1]
  EXPECT_EQ(list.front().id, 2);
  EXPECT_EQ(list.back().id, 1);

  list.push_front(d3);  // List: [3, 2, 1]
  EXPECT_EQ(list.front().id, 3);
  EXPECT_EQ(list.size(), 3);
}

TEST_F(IntrusiveListTest, PopFront_FIFO_WhenCombinedWithPushBack) {
  // Queue behavior
  list.push_back(d1);
  list.push_back(d2);

  list.push_back(d3);

  TestData result{0, ""};

  list.pop_front(result);
  EXPECT_EQ(result.id, 1);

  list.pop_front(result);
  EXPECT_EQ(result.id, 2);

  EXPECT_EQ(list.size(), 1);
}

// -----------------------------------------------------------------------------
// Section 4: Random Access (Operator [])
// -----------------------------------------------------------------------------

TEST_F(IntrusiveListTest, OperatorBrackets_Access) {
  list.push_back(d1);  // Index 0
  list.push_back(d2);  // Index 1
  list.push_back(d3);  // Index 2

  EXPECT_EQ(list[0].id, 1);
  EXPECT_EQ(list[1].id, 2);
  EXPECT_EQ(list[2].id, 3);
}

TEST_F(IntrusiveListTest, OperatorBrackets_ThrowsOutOfBounds) {
  list.push_back(d1);
  EXPECT_THROW(list[1], std::out_of_range);
  EXPECT_THROW(list[100], std::out_of_range);
}

// -----------------------------------------------------------------------------
// Section 5: Removal from Arbitrary Positions
// -----------------------------------------------------------------------------

TEST_F(IntrusiveListTest, Remove_MiddleNode) {
  list.push_back(d1);
  list.push_back(d2);
  list.push_back(d3);

  // Remove d2 directly (intrusive capability)
  list.remove(d2);

  EXPECT_EQ(list.size(), 2);
  EXPECT_EQ(list.front().id, 1);
  EXPECT_EQ(list.back().id, 3);

  // Verify linkage: d1 next should be d3
  // We check this via operator[] or pop logic
  EXPECT_EQ(list[1].id, 3);
}

TEST_F(IntrusiveListTest, Remove_HeadNode) {
  list.push_back(d1);
  list.push_back(d2);

  list.remove(d1);

  EXPECT_EQ(list.size(), 1);
  EXPECT_EQ(list.front().id, 2);
}

TEST_F(IntrusiveListTest, Remove_TailNode) {
  list.push_back(d1);
  list.push_back(d2);

  list.remove(d2);

  EXPECT_EQ(list.size(), 1);
  EXPECT_EQ(list.back().id, 1);
}

// -----------------------------------------------------------------------------
// Section 6: Edge Cases & Safety
// -----------------------------------------------------------------------------

TEST_F(IntrusiveListTest, EmptyList_ExceptionHandling) {
  EXPECT_EQ(list.size(), 0);

  TestData dummy{0, ""};
  EXPECT_THROW(list.pop_back(dummy), std::out_of_range);
  EXPECT_THROW(list.pop_front(dummy), std::out_of_range);
  EXPECT_THROW(list[0], std::out_of_range);
}

TEST_F(IntrusiveListTest, SingleElement_PushPop) {
  list.push_back(d1);
  EXPECT_EQ(list.size(), 1);
  EXPECT_EQ(list.front().id, 1);
  EXPECT_EQ(list.back().id, 1);

  TestData res{0, ""};
  list.pop_front(res);
  EXPECT_EQ(res.id, 1);
  EXPECT_EQ(list.size(), 0);
}

// -----------------------------------------------------------------------------
// Section 7: Reusability
// -----------------------------------------------------------------------------

TEST_F(IntrusiveListTest, Node_Reuse) {
  // Push d1, remove it, push it again.
  // Intrusive nodes persist, but their pointers must be reset correctly.

  list.push_back(d1);
  list.remove(d1);
  EXPECT_EQ(list.size(), 0);

  list.push_back(d1);
  EXPECT_EQ(list.size(), 1);
  EXPECT_EQ(list.front().id, 1);
}
TEST_F(IntrusiveListTest, MoveSemantics_SentinelUpdate) {
  // 1. Setup List A
  intrusive_list<TestData> listA;
  TestData item1{1, "Moved"};
  listA.push_back(item1);

  // 2. Move A to List B
  intrusive_list<TestData> listB = std::move(listA);

  // 3. Verify List A is empty (optional, depends on your design)
  EXPECT_EQ(listA.size(), 0);

  // 4. Verify List B has the item
  EXPECT_EQ(listB.size(), 1);
  EXPECT_EQ(listB.back().id, 1);

  // 5. CRITICAL: Verify the linkage integrity
  // If the node still points to A's sentinel, this check or subsequent usage
  // will fail (especially if we force A's destruction here by scoping)
  {
    intrusive_list<TestData> listTemp;
    TestData tempItem{99, "Temp"};
    listTemp.push_back(tempItem);

    intrusive_list<TestData> listMoved = std::move(listTemp);
    // listTemp dies here.
    // If tempItem.prev still points to listTemp.root,
    // accessing listMoved.back() might crash.
  }
}
TEST_F(IntrusiveListTest, Iterator_RangeBasedFor) {
  list.push_back(d1);  // id 1
  list.push_back(d2);  // id 2
  list.push_back(d3);  // id 3

  int sum = 0;
  // This uses begin(), end(), operator++, operator*
  for (auto& item : list) {
    sum += item.id;
  }

  EXPECT_EQ(sum, 6);
}

TEST_F(IntrusiveListTest, Iterator_StdAlgorithm) {
  list.push_back(d1);
  list.push_back(d2);

  // Test compatibility with std::find_if
  auto it = std::find_if(list.begin(), list.end(),
                         [](const TestData& item) { return item.id == 2; });

  ASSERT_NE(it, list.end());
  EXPECT_EQ(it->id, 2);
  EXPECT_EQ(it->name, "Two");
}
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
