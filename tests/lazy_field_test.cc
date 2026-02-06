#include <gtest/gtest.h>
#include "protoopt/lazy_field.h"
#include "google/protobuf/arena.h"

// Mock message for testing
struct MockMessage {
  int32_t value = 0;
  std::string name;
  
  bool ParseFromString(const std::string& data) {
    // Simple mock parsing
    if (data.size() >= 4) {
      value = *reinterpret_cast<const int32_t*>(data.data());
      return true;
    }
    return false;
  }
  
  size_t ByteSizeLong() const {
    return 4 + name.size();
  }
};

using namespace protoopt;
using google::protobuf::Arena;

class LazyFieldTest : public ::testing::Test {
 protected:
  Arena arena;
};

TEST_F(LazyFieldTest, DefaultConstruction) {
  LazyField<MockMessage> field;
  EXPECT_FALSE(field.IsInitialized());
}

TEST_F(LazyFieldTest, ArenaConstruction) {
  LazyField<MockMessage> field(&arena);
  EXPECT_FALSE(field.IsInitialized());
}

TEST_F(LazyFieldTest, DeferredParsing) {
  LazyField<MockMessage> field(&arena);
  
  // Simulate parsing raw bytes
  std::string raw_bytes(10, 'x');
  // ParseFrom would be called during message deserialization
  
  // Before access, message is not parsed
  EXPECT_FALSE(field.IsInitialized());
  
  // First access triggers parsing
  const auto& msg = field.Get();
  // After Get(), field should be initialized (even if parsing failed)
  EXPECT_TRUE(field.IsInitialized());
}

TEST_F(LazyFieldTest, MutableMarksDirty) {
  LazyField<MockMessage> field(&arena);
  
  auto* mutable_msg = field.Mutable();
  mutable_msg->value = 42;
  
  const auto& msg = field.Get();
  EXPECT_EQ(msg.value, 42);
}

TEST_F(LazyFieldTest, Clear) {
  LazyField<MockMessage> field(&arena);
  field.Mutable()->value = 100;
  EXPECT_TRUE(field.IsInitialized());
  
  field.Clear();
  EXPECT_FALSE(field.IsInitialized());
}

TEST_F(LazyFieldTest, ByteSizeBeforeParse) {
  LazyField<MockMessage> field(&arena);
  // Should return cached raw bytes size before parsing
  EXPECT_EQ(field.ByteSizeLong(), 0);  // Not initialized yet
}
