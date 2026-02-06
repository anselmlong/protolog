// Copyright 2026

#ifndef PROTOOPT_ARENA_STRING_H_
#define PROTOOPT_ARENA_STRING_H_

#include <cstddef>
#include <cstdint>
#include <string>

#if __has_include("absl/strings/string_view.h")
#include "absl/strings/string_view.h"
#else
#include <string_view>
namespace absl {
using string_view = std::string_view;
}  // namespace absl
#endif

namespace google {
namespace protobuf {
class Arena;
}  // namespace protobuf
}  // namespace google

namespace protoopt {

class ArenaString {
 public:
  using Arena = ::google::protobuf::Arena;

  ArenaString();
  explicit ArenaString(Arena* arena);
  ~ArenaString();

  ArenaString(const ArenaString&) = delete;
  ArenaString& operator=(const ArenaString&) = delete;

  absl::string_view Get() const;
  void Set(absl::string_view value, Arena* arena);
  std::string* Mutable(Arena* arena);
  void Clear();
  bool IsDefault() const;
  size_t Size() const;

 private:
  enum Flags : uintptr_t {
    kArenaBit = 0x1,
    kMutableBit = 0x2,
    kMask = 0x3,
  };

  enum Type : uintptr_t {
    kDefault = 0,
    kFixedSizeArena = 1,
    kAllocated = 2,
    kMutableArena = 3,
  };

  static const std::string* DefaultString();

  static uintptr_t Encode(const std::string* ptr, Type type);
  static const std::string* DecodePtr(uintptr_t tagged_ptr);
  static std::string* DecodeMutablePtr(uintptr_t tagged_ptr);
  static Type DecodeType(uintptr_t tagged_ptr);

  const std::string* Ptr() const;
  Type CurrentType() const;
  void SetTagged(const std::string* ptr, Type type);
  void Destroy();
  void SetDefault();

  uintptr_t tagged_ptr_;
};

}  // namespace protoopt

#endif  // PROTOOPT_ARENA_STRING_H_
