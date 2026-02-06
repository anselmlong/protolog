#include <gtest/gtest.h>
#include "protoopt/arena_string.h"
#include "google/protobuf/arena.h"

using namespace protoopt;
using google::protobuf::Arena;

class ArenaStringTest : public ::testing::Test {
 protected:
  Arena arena;
};

TEST_F(ArenaStringTest, DefaultConstruction) {
  ArenaString str;
  EXPECT_TRUE(str.IsDefault());
  EXPECT_EQ(str.Size(), 0);
  EXPECT_EQ(str.Get(), "");
}

TEST_F(ArenaStringTest, ArenaConstruction) {
  ArenaString str(&arena);
  EXPECT_TRUE(str.IsDefault());
  EXPECT_EQ(str.Get(), "");
}

TEST_F(ArenaStringTest, SetAndGet) {
  ArenaString str;
  str.Set("hello world", &arena);
  EXPECT_EQ(str.Get(), "hello world");
  EXPECT_EQ(str.Size(), 11);
}

TEST_F(ArenaStringTest, SetEmpty) {
  ArenaString str;
  str.Set("", &arena);
  EXPECT_EQ(str.Get(), "");
  EXPECT_FALSE(str.IsDefault());
}

TEST_F(ArenaStringTest, MutableModifies) {
  ArenaString str(&arena);
  auto* mutable_str = str.Mutable(&arena);
  *mutable_str = "modified";
  EXPECT_EQ(str.Get(), "modified");
}

TEST_F(ArenaStringTest, ClearResetsToDefault) {
  ArenaString str(&arena);
  str.Set("test", &arena);
  str.Clear();
  EXPECT_TRUE(str.IsDefault());
  EXPECT_EQ(str.Get(), "");
}

TEST_F(ArenaStringTest, StringViewAccess) {
  ArenaString str(&arena);
  str.Set("string_view_test", &arena);
  absl::string_view sv = str.Get();
  EXPECT_EQ(sv, "string_view_test");
  EXPECT_EQ(sv.size(), 16);
}
