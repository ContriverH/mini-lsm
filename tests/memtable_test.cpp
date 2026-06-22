// Test file for MemTable.
//
// We use GoogleTest (gtest), which is the most common C++ testing framework.
// It's similar to Jest in the JS world.
//
// Key differences from Jest:
//   - TEST(SuiteName, TestName) = describe("SuiteName", () => it("TestName", ...))
//   - EXPECT_EQ(a, b) = expect(a).toBe(b)         (non-fatal: test continues)
//   - ASSERT_TRUE(x)  = expect(x).toBe(true)       (fatal: test stops on failure)
//   - EXPECT vs ASSERT: EXPECT logs the failure but keeps running the test.
//     ASSERT stops the test immediately. Use ASSERT when subsequent lines
//     would crash if the assertion failed (e.g., dereferencing a null pointer).

// Include the code we're testing.
#include "memtable.h"

// GoogleTest header. GTest::gtest_main (linked in CMakeLists.txt) provides
// the main() function automatically — we don't need to write one.
#include <gtest/gtest.h>

// Tests live in the same namespace as the code they test, so we can
// access types like MemTable and RecordType without the mini_lsm:: prefix.
namespace mini_lsm {

// TEST(TestSuiteName, TestCaseName) — registers a test case.
// GoogleTest auto-discovers all TEST() macros and runs them.
TEST(MemTableTest, PutThenGetReturnsValue) {
  // Create a MemTable with default flush threshold (4MB).
  // In C++, this creates the object ON THE STACK — no "new" needed.
  // It will be automatically destroyed when this function returns.
  MemTable mt;

  // Insert a key-value pair.
  mt.Put("hello", "world");

  // Prepare output variables. In C++, we declare variables first,
  // then pass their addresses (&) to functions that fill them in.
  // This is the "out parameter" pattern — the function writes its
  // result through the pointer.
  std::string value;
  bool found_tombstone = false;

  // ASSERT_TRUE: if Get() returns false, the test STOPS here.
  // We use ASSERT (not EXPECT) because the lines below would be
  // meaningless if Get() failed — value would be uninitialized.
  //
  // "&value" and "&found_tombstone" = "address of" operator.
  // We pass the memory address of these variables so Get() can
  // write to them. In JS terms, it's like passing a mutable object
  // that the function modifies: get("hello", { value, found_tombstone }).
  ASSERT_TRUE(mt.Get("hello", &value, &found_tombstone));

  // EXPECT_EQ: non-fatal check. If this fails, the test continues
  // (though it will still be reported as failed).
  EXPECT_EQ(value, "world");
  EXPECT_FALSE(found_tombstone);
}

}  // namespace mini_lsm
