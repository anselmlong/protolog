// Copyright 2026
#ifndef PROTOOPT_UNKNOWN_FIELD_SKIPPER_H_
#define PROTOOPT_UNKNOWN_FIELD_SKIPPER_H_

#include <cstdint>
#include <limits>

#if defined(__has_include)
#if __has_include(<google/protobuf/io/coded_stream.h>)
#include <google/protobuf/io/coded_stream.h>
#define PROTOOPT_HAS_CODED_STREAM 1
#endif
#if __has_include(<google/protobuf/parse_context.h>)
#include <google/protobuf/parse_context.h>
#define PROTOOPT_HAS_PARSE_CONTEXT 1
#endif
#if __has_include(<google/protobuf/wire_format_lite.h>)
#include <google/protobuf/wire_format_lite.h>
#define PROTOOPT_HAS_WIRE_FORMAT_LITE 1
#endif
#endif

namespace google {
namespace protobuf {

#if !defined(PROTOOPT_HAS_CODED_STREAM)
namespace io {
class CodedInputStream {
 public:
  uint32_t ReadTag();
  bool ReadVarint64(uint64_t* value);
  bool ReadVarint32(uint32_t* value);
  bool ReadLittleEndian64(uint64_t* value);
  bool ReadLittleEndian32(uint32_t* value);
  bool Skip(int count);
};
}  // namespace io
#endif

namespace internal {
#if !defined(PROTOOPT_HAS_PARSE_CONTEXT)
class ParseContext;
#endif
#if !defined(PROTOOPT_HAS_WIRE_FORMAT_LITE)
struct WireFormatLite {
  enum WireType {
    WIRETYPE_VARINT = 0,
    WIRETYPE_FIXED64 = 1,
    WIRETYPE_LENGTH_DELIMITED = 2,
    WIRETYPE_START_GROUP = 3,
    WIRETYPE_END_GROUP = 4,
    WIRETYPE_FIXED32 = 5,
  };
};
#endif
}  // namespace internal

}  // namespace protobuf
}  // namespace google

namespace protoopt {

using ParseContext = ::google::protobuf::internal::ParseContext;

class UnknownFieldSkipper {
 public:
  static inline bool SkipField(::google::protobuf::io::CodedInputStream* input,
                               uint32_t tag) {
    using WireType = ::google::protobuf::internal::WireFormatLite::WireType;

    switch (static_cast<WireType>(tag & 0x7u)) {
      case ::google::protobuf::internal::WireFormatLite::WIRETYPE_VARINT:
        return SkipVarint(input);
      case ::google::protobuf::internal::WireFormatLite::WIRETYPE_FIXED64:
        return SkipFixed64(input);
      case ::google::protobuf::internal::WireFormatLite::WIRETYPE_LENGTH_DELIMITED:
        return SkipLengthDelimited(input);
      case ::google::protobuf::internal::WireFormatLite::WIRETYPE_START_GROUP:
        return SkipGroup(input, tag);
      case ::google::protobuf::internal::WireFormatLite::WIRETYPE_END_GROUP:
        return false;
      case ::google::protobuf::internal::WireFormatLite::WIRETYPE_FIXED32:
        return SkipFixed32(input);
      default:
        return false;
    }
  }

  static inline bool SkipMessage(
      ::google::protobuf::io::CodedInputStream* input) {
    while (true) {
      const uint32_t tag = input->ReadTag();
      if (tag == 0) {
        return true;
      }
      if ((tag & 0x7u) ==
          ::google::protobuf::internal::WireFormatLite::WIRETYPE_END_GROUP) {
        return true;
      }
      if (!SkipField(input, tag)) {
        return false;
      }
    }
  }

  static inline bool SkipVarint(::google::protobuf::io::CodedInputStream* input) {
    uint64_t value = 0;
    return input->ReadVarint64(&value);
  }

  static inline bool SkipFixed64(
      ::google::protobuf::io::CodedInputStream* input) {
    uint64_t value = 0;
    return input->ReadLittleEndian64(&value);
  }

  static inline bool SkipLengthDelimited(
      ::google::protobuf::io::CodedInputStream* input) {
    uint32_t size = 0;
    return input->ReadVarint32(&size) && input->Skip(static_cast<int>(size));
  }

  static inline bool SkipFixed32(
      ::google::protobuf::io::CodedInputStream* input) {
    uint32_t value = 0;
    return input->ReadLittleEndian32(&value);
  }

  static inline bool SkipGroup(::google::protobuf::io::CodedInputStream* input,
                               uint32_t start_tag) {
    const uint32_t start_field_number = start_tag >> 3;
    while (true) {
      const uint32_t tag = input->ReadTag();
      if (tag == 0) {
        return false;
      }

      const uint32_t wire_type = tag & 0x7u;
      if (wire_type ==
          ::google::protobuf::internal::WireFormatLite::WIRETYPE_END_GROUP) {
        return (tag >> 3) == start_field_number;
      }

      if (!SkipField(input, tag)) {
        return false;
      }
    }
  }

  static inline const char* SkipVarintInline(const char* ptr) {
    int count = 0;
    while ((static_cast<uint8_t>(*ptr) & 0x80u) != 0u) {
      ++ptr;
      ++count;
      if (count >= 10) {
        return nullptr;
      }
    }
    return ptr + 1;
  }

  static inline const char* SkipFixed64Inline(const char* ptr) {
    return ptr + 8;
  }

  static inline const char* SkipFixed32Inline(const char* ptr) {
    return ptr + 4;
  }

  static inline const char* SkipFieldInline(const char* ptr, uint32_t tag,
                                            ParseContext* ctx) {
    static_cast<void>(ctx);

    switch (tag & 0x7u) {
      case ::google::protobuf::internal::WireFormatLite::WIRETYPE_VARINT:
        return SkipVarintInline(ptr);
      case ::google::protobuf::internal::WireFormatLite::WIRETYPE_FIXED64:
        return SkipFixed64Inline(ptr);
      case ::google::protobuf::internal::WireFormatLite::WIRETYPE_LENGTH_DELIMITED:
        return SkipLengthDelimitedInline(ptr);
      case ::google::protobuf::internal::WireFormatLite::WIRETYPE_START_GROUP:
        return SkipGroupInline(ptr, tag, ctx);
      case ::google::protobuf::internal::WireFormatLite::WIRETYPE_END_GROUP:
        return nullptr;
      case ::google::protobuf::internal::WireFormatLite::WIRETYPE_FIXED32:
        return SkipFixed32Inline(ptr);
      default:
        return nullptr;
    }
  }

 private:
  static inline const char* ReadVarint32Inline(const char* ptr,
                                               uint32_t* value) {
    uint64_t out = 0;
    ptr = ReadVarint64Inline(ptr, &out);
    if (ptr == nullptr || out > std::numeric_limits<uint32_t>::max()) {
      return nullptr;
    }
    *value = static_cast<uint32_t>(out);
    return ptr;
  }

  static inline const char* ReadVarint64Inline(const char* ptr,
                                               uint64_t* value) {
    uint64_t out = 0;
    uint32_t shift = 0;
    for (int i = 0; i < 10; ++i) {
      const uint8_t byte = static_cast<uint8_t>(*ptr++);
      out |= static_cast<uint64_t>(byte & 0x7Fu) << shift;
      if ((byte & 0x80u) == 0u) {
        *value = out;
        return ptr;
      }
      shift += 7;
    }
    return nullptr;
  }

  static inline const char* ReadTagInline(const char* ptr, uint32_t* tag) {
    return ReadVarint32Inline(ptr, tag);
  }

  static inline const char* SkipLengthDelimitedInline(const char* ptr) {
    uint32_t size = 0;
    ptr = ReadVarint32Inline(ptr, &size);
    if (ptr == nullptr) {
      return nullptr;
    }
    return ptr + size;
  }

  static inline const char* SkipGroupInline(const char* ptr, uint32_t start_tag,
                                            ParseContext* ctx) {
    static_cast<void>(ctx);
    const uint32_t start_field_number = start_tag >> 3;

    while (true) {
      uint32_t tag = 0;
      ptr = ReadTagInline(ptr, &tag);
      if (ptr == nullptr || tag == 0) {
        return nullptr;
      }

      const uint32_t wire_type = tag & 0x7u;
      if (wire_type ==
          ::google::protobuf::internal::WireFormatLite::WIRETYPE_END_GROUP) {
        return ((tag >> 3) == start_field_number) ? ptr : nullptr;
      }

      ptr = SkipFieldInline(ptr, tag, ctx);
      if (ptr == nullptr) {
        return nullptr;
      }
    }
  }
};

}  // namespace protoopt

#endif  // PROTOOPT_UNKNOWN_FIELD_SKIPPER_H_
