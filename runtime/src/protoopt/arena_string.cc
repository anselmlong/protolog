#include "protoopt/arena_string.h"

#include <cassert>
#include <cstdint>
#include <utility>

#if __has_include("google/protobuf/arena.h")
#include "google/protobuf/arena.h"
#define PROTOOPT_HAS_PROTOBUF_ARENA 1
#else
#define PROTOOPT_HAS_PROTOBUF_ARENA 0
#endif

namespace protoopt {

namespace {

struct Allocation {
  std::string* ptr;
  bool on_arena;
};

Allocation AllocateString(ArenaString::Arena* arena) {
#if PROTOOPT_HAS_PROTOBUF_ARENA
  if (arena == nullptr) {
    return {new std::string(), false};
  }
  return {::google::protobuf::Arena::Create<std::string>(arena), true};
#else
  static_cast<void>(arena);
  return {new std::string(), false};
#endif
}

}  // namespace

ArenaString::ArenaString() : tagged_ptr_(Encode(DefaultString(), kDefault)) {}

ArenaString::ArenaString(Arena* arena) : tagged_ptr_(Encode(DefaultString(), kDefault)) {
  if (arena != nullptr) {
    const Allocation allocation = AllocateString(arena);
    SetTagged(allocation.ptr, allocation.on_arena ? kMutableArena : kAllocated);
  }
}

ArenaString::~ArenaString() { Destroy(); }

absl::string_view ArenaString::Get() const { return *Ptr(); }

void ArenaString::Set(absl::string_view value, Arena* arena) {
  if (value.empty() && IsDefault()) {
    return;
  }

  std::string* ptr = Mutable(arena);
  ptr->assign(value.data(), value.size());
}

std::string* ArenaString::Mutable(Arena* arena) {
  switch (CurrentType()) {
    case kAllocated:
    case kMutableArena:
      return DecodeMutablePtr(tagged_ptr_);
    case kDefault: {
      const Allocation allocation = AllocateString(arena);
      SetTagged(allocation.ptr, allocation.on_arena ? kMutableArena : kAllocated);
      return allocation.ptr;
    }
    case kFixedSizeArena: {
      const std::string* current = Ptr();
      const Allocation allocation = AllocateString(arena);
      allocation.ptr->assign(*current);
      SetTagged(allocation.ptr, allocation.on_arena ? kMutableArena : kAllocated);
      return allocation.ptr;
    }
  }

  return nullptr;
}

void ArenaString::Clear() {
  switch (CurrentType()) {
    case kDefault:
      return;
    case kAllocated:
    case kMutableArena:
      DecodeMutablePtr(tagged_ptr_)->clear();
      return;
    case kFixedSizeArena:
      SetDefault();
      return;
  }
}

bool ArenaString::IsDefault() const { return CurrentType() == kDefault; }

size_t ArenaString::Size() const { return Ptr()->size(); }

const std::string* ArenaString::DefaultString() {
  static const std::string* kDefaultString = new std::string();
  return kDefaultString;
}

uintptr_t ArenaString::Encode(const std::string* ptr, Type type) {
  assert(ptr != nullptr);
  const uintptr_t raw_ptr = reinterpret_cast<uintptr_t>(ptr);
  assert((raw_ptr & kMask) == 0);
  return raw_ptr | static_cast<uintptr_t>(type);
}

const std::string* ArenaString::DecodePtr(uintptr_t tagged_ptr) {
  return reinterpret_cast<const std::string*>(tagged_ptr & ~kMask);
}

std::string* ArenaString::DecodeMutablePtr(uintptr_t tagged_ptr) {
  return const_cast<std::string*>(DecodePtr(tagged_ptr));
}

ArenaString::Type ArenaString::DecodeType(uintptr_t tagged_ptr) {
  return static_cast<Type>(tagged_ptr & kMask);
}

const std::string* ArenaString::Ptr() const { return DecodePtr(tagged_ptr_); }

ArenaString::Type ArenaString::CurrentType() const { return DecodeType(tagged_ptr_); }

void ArenaString::SetTagged(const std::string* ptr, Type type) {
  if (ptr == DecodePtr(tagged_ptr_) && type == CurrentType()) {
    return;
  }

  if (CurrentType() == kAllocated) {
    delete DecodeMutablePtr(tagged_ptr_);
  }

  tagged_ptr_ = Encode(ptr, type);
}

void ArenaString::Destroy() {
  if (CurrentType() == kAllocated) {
    delete DecodeMutablePtr(tagged_ptr_);
  }
  SetDefault();
}

void ArenaString::SetDefault() { tagged_ptr_ = Encode(DefaultString(), kDefault); }

}  // namespace protoopt
