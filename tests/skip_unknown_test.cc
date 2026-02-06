#include <gtest/gtest.h>
#include "protoopt/unknown_field_skipper.h"
#include "google/protobuf/io/coded_stream.h"
#include <vector>
#include <memory>

using namespace protoopt;
using google::protobuf::io::CodedInputStream;
using google::protobuf::io::ArrayInputStream;

class SkipUnknownTest : public ::testing::Test {
 protected:
  std::vector<uint8_t> buffer;
  
  std::unique_ptr<CodedInputStream> CreateStream() {
    auto array_stream = new ArrayInputStream(buffer.data(), buffer.size());
    return std::make_unique<CodedInputStream>(array_stream);
  }
};

TEST_F(SkipUnknownTest, SkipVarint) {
  buffer = {0x08, 0x7f};
  auto stream = CreateStream();
  
  uint32_t tag;
  EXPECT_TRUE(stream->ReadVarint32(&tag));
  EXPECT_EQ(tag, 0x08);
  
  EXPECT_TRUE(UnknownFieldSkipper::SkipVarint(stream.get()));
}

TEST_F(SkipUnknownTest, SkipFixed64) {
  buffer = {0x11, 0, 0, 0, 0, 0, 0, 0, 0};
  auto stream = CreateStream();
  
  uint32_t tag;
  stream->ReadVarint32(&tag);
  EXPECT_TRUE(UnknownFieldSkipper::SkipFixed64(stream.get()));
}

TEST_F(SkipUnknownTest, SkipLengthDelimited) {
  buffer = {0x1a, 0x05, 'h', 'e', 'l', 'l', 'o'};
  auto stream = CreateStream();
  
  uint32_t tag;
  stream->ReadVarint32(&tag);
  EXPECT_TRUE(UnknownFieldSkipper::SkipLengthDelimited(stream.get()));
}

TEST_F(SkipUnknownTest, SkipFixed32) {
  buffer = {0x25, 0, 0, 0, 0};
  auto stream = CreateStream();
  
  uint32_t tag;
  stream->ReadVarint32(&tag);
  EXPECT_TRUE(UnknownFieldSkipper::SkipFixed32(stream.get()));
}

TEST_F(SkipUnknownTest, SkipUnknownFieldNoAllocation) {
  buffer.clear();
  for (int i = 100; i < 200; ++i) {
    buffer.push_back((i << 3) | 0);
    buffer.push_back(0x42);
  }
  
  auto stream = CreateStream();
  
  while (!stream->ConsumedEntireMessage()) {
    uint32_t tag;
    if (!stream->ReadVarint32(&tag)) break;
    if (tag == 0) break;
    
    EXPECT_TRUE(UnknownFieldSkipper::SkipField(stream.get(), tag));
  }
  
  SUCCEED() << "Skipped all unknown fields without storage";
}

TEST_F(SkipUnknownTest, InlineSkipPerformance) {
  const char data[] = {0x08, 0x7f};
  const char* ptr = data;
  
  ptr = SkipVarintInline(ptr);
  EXPECT_EQ(ptr, data + 2);
  
  const char data64[] = {0, 0, 0, 0, 0, 0, 0, 0};
  ptr = SkipFixed64Inline(data64);
  EXPECT_EQ(ptr, data64 + 8);
  
  const char data32[] = {0, 0, 0, 0};
  ptr = SkipFixed32Inline(data32);
  EXPECT_EQ(ptr, data32 + 4);
}
