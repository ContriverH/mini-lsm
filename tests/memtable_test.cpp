#include "memtable.h"

#include <gtest/gtest.h>

namespace mini_lsm {

TEST(MemTableTest, PutThenGetReturnsValue) {
  MemTable mt;
  mt.Put("hello", "world");

  std::string value;
  bool found_tombstone = false;
  ASSERT_TRUE(mt.Get("hello", &value, &found_tombstone));
  EXPECT_EQ(value, "world");
  EXPECT_FALSE(found_tombstone);
}

}  // namespace mini_lsm
