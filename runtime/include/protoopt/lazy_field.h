// Copyright 2026
#ifndef PROTOOPT_LAZY_FIELD_H_
#define PROTOOPT_LAZY_FIELD_H_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <type_traits>

namespace google {
namespace protobuf {
class Arena;
namespace internal {
class ParseContext;
}  // namespace internal
namespace io {
class EpsCopyOutputStream;
}  // namespace io
}  // namespace protobuf
}  // namespace google

namespace protoopt {

template <typename MessageType>
class LazyField {
 public:
  using Arena = ::google::protobuf::Arena;
  using ParseContext = ::google::protobuf::internal::ParseContext;

  LazyField()
      : state_(UNPARSED),
        message_(nullptr),
        arena_(nullptr),
        has_value_(false) {}

  explicit LazyField(Arena* arena)
      : state_(UNPARSED),
        message_(nullptr),
        arena_(arena),
        has_value_(false) {}

  // Lock-free read optimization: check state without locking
  bool IsParsed() const {
    return state_.load(std::memory_order_acquire) != UNPARSED;
  }

  ~LazyField() {
    if (arena_ == nullptr) {
      delete message_;
    }
  }

  const char* ParseFrom(const char* ptr, ParseContext* ctx, Arena* arena) {
    (void)ctx;
    if (ptr == nullptr) {
      return nullptr;
    }

    uint32_t size = 0;
    const char* payload = ReadVarint32(ptr, &size);
    if (payload == nullptr) {
      return nullptr;
    }

    std::lock_guard<std::mutex> lock(mu_);
    
    // Arena reassignment safety: If we're changing arenas and have a parsed message,
    // we need to handle the transition properly. Messages are tied to their arena.
    if (arena != nullptr && arena_ != nullptr && arena_ != arena && message_ != nullptr) {
      // Cannot easily move between arenas. Clear the message and re-parse on new arena.
      // The old arena keeps ownership of the old message.
      message_ = nullptr;
    }
    
    // If switching from heap to arena, delete heap-allocated message
    if (arena_ == nullptr && arena != nullptr && message_ != nullptr) {
      delete message_;
      message_ = nullptr;
    }
    
    // Only set arena if we don't have one yet, or if explicitly provided
    if (arena_ == nullptr && arena != nullptr) {
      arena_ = arena;
    }

    unparsed_bytes_.assign(payload, static_cast<size_t>(size));
    state_.store(UNPARSED, std::memory_order_release);
    has_value_ = true;
    return payload + size;
  }

  const MessageType& Get() const {
    EnsureParsed();
    return *message_;
  }

  MessageType* Mutable() {
    EnsureParsedNonConst();
    return message_;
  }

  bool IsInitialized() const {
    if (!has_value_) {
      return true;
    }
    EnsureParsed();
    return message_ != nullptr && message_->IsInitialized();
  }

  uint8_t* Serialize(uint8_t* target,
                     ::google::protobuf::io::EpsCopyOutputStream* stream) const {
    (void)stream;
    if (target == nullptr) {
      return nullptr;
    }

    std::lock_guard<std::mutex> lock(mu_);
    if (!has_value_) {
      return target;
    }

    if (state_ == UNPARSED) {
      return WriteLengthDelimitedBytes(target, unparsed_bytes_);
    }

    if (message_ == nullptr) {
      return target;
    }

    const size_t payload_size = message_->ByteSizeLong();
    target = WriteVarint32(target, static_cast<uint32_t>(payload_size));
    return message_->SerializeWithCachedSizesToArray(target);
  }

  size_t ByteSizeLong() const {
    std::lock_guard<std::mutex> lock(mu_);
    if (!has_value_) {
      return 0;
    }

    if (state_ == UNPARSED) {
      return Varint32Size(static_cast<uint32_t>(unparsed_bytes_.size())) +
             unparsed_bytes_.size();
    }

    if (message_ == nullptr) {
      return 0;
    }

    const size_t payload_size = message_->ByteSizeLong();
    return Varint32Size(static_cast<uint32_t>(payload_size)) + payload_size;
  }

  void Clear() {
    std::lock_guard<std::mutex> lock(mu_);
    unparsed_bytes_.clear();
    has_value_ = false;
    state_.store(UNPARSED, std::memory_order_release);

    if (message_ != nullptr) {
      message_->Clear();
    }
  }

  void SetAllocated(MessageType* message, Arena* arena) {
    std::lock_guard<std::mutex> lock(mu_);

    if (message_ != nullptr && message_ != message && arena_ == nullptr) {
      delete message_;
    }

    message_ = message;
    arena_ = arena;
    has_value_ = (message != nullptr);
    unparsed_bytes_.clear();
    state_.store(has_value_ ? DIRTY : UNPARSED, std::memory_order_release);
  }

 private:
  enum State { UNPARSED, PARSED, DIRTY };

  void EnsureParsed() const {
    // Fast path: already parsed, no lock needed
    if (IsParsed()) return;
    // Slow path: need to parse under lock
    std::lock_guard<std::mutex> lock(mu_);
    EnsureParsedLocked();
  }

  void EnsureParsedNonConst() {
    // Fast path: already parsed and marked dirty
    State current = state_.load(std::memory_order_acquire);
    if (current == DIRTY && message_ != nullptr) return;
    // Slow path: need to parse under lock
    std::lock_guard<std::mutex> lock(mu_);
    EnsureParsedLocked();
    current = state_.load(std::memory_order_relaxed);
    if (current != DIRTY) {
      state_.store(DIRTY, std::memory_order_release);
      unparsed_bytes_.clear();
    }
    has_value_ = true;
  }

  void EnsureParsedLocked() const {
    if (message_ == nullptr) {
      message_ = CreateMessage();
    }

    if (!has_value_) {
      state_ = PARSED;
      return;
    }

    if (state_ != UNPARSED) {
      return;
    }

    if (unparsed_bytes_.empty()) {
      message_->Clear();
      state_.store(PARSED, std::memory_order_release);
      return;
    }

    if (!message_->ParseFromArray(unparsed_bytes_.data(),
                                  static_cast<int>(unparsed_bytes_.size()))) {
      message_->Clear();
    }
    state_.store(PARSED, std::memory_order_release);
  }

  MessageType* CreateMessage() const {
#if PROTOOPT_HAS_PROTOBUF_ARENA
    if (arena_ != nullptr) {
      return ::google::protobuf::Arena::CreateMessage<MessageType>(arena_);
    }
#endif
    return new MessageType();
  }

  static const char* ReadVarint32(const char* ptr, uint32_t* value) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(ptr);
    uint32_t result = 0;
    uint32_t shift = 0;
    
    // Unrolled loop for maximum speed - varint32 maxes at 5 bytes
    #define PROTOOPT_READ_BYTE32 \
      do { \
        const uint8_t byte = *p++; \
        result |= static_cast<uint32_t>(byte & 0x7Fu) << shift; \
        if ((byte & 0x80u) == 0u) { \
          *value = result; \
          return reinterpret_cast<const char*>(p); \
        } \
        shift += 7; \
      } while (0)
    
    PROTOOPT_READ_BYTE32;
    PROTOOPT_READ_BYTE32;
    PROTOOPT_READ_BYTE32;
    PROTOOPT_READ_BYTE32;
    PROTOOPT_READ_BYTE32;
    
    #undef PROTOOPT_READ_BYTE32
    return nullptr;
  }

  static size_t Varint32Size(uint32_t value) {
    size_t size = 1;
    while (value >= 0x80u) {
      value >>= 7;
      ++size;
    }
    return size;
  }

  static uint8_t* WriteVarint32(uint8_t* target, uint32_t value) {
    while (value >= 0x80u) {
      *target++ = static_cast<uint8_t>((value & 0x7Fu) | 0x80u);
      value >>= 7;
    }
    *target++ = static_cast<uint8_t>(value);
    return target;
  }

  static uint8_t* WriteLengthDelimitedBytes(uint8_t* target,
                                            const std::string& bytes) {
    target = WriteVarint32(target, static_cast<uint32_t>(bytes.size()));
    if (!bytes.empty()) {
      std::memcpy(target, bytes.data(), bytes.size());
      target += bytes.size();
    }
    return target;
  }

  mutable std::mutex mu_;
  mutable std::atomic<State> state_;
  mutable std::string unparsed_bytes_;
  mutable MessageType* message_;
  Arena* arena_;
  mutable bool has_value_;
};

}  // namespace protoopt

#endif  // PROTOOPT_LAZY_FIELD_H_
