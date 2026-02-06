// Copyright 2026
#ifndef PROTOOPT_LAZY_FIELD_H_
#define PROTOOPT_LAZY_FIELD_H_

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
    if (arena_ == nullptr && arena != nullptr && message_ != nullptr) {
      delete message_;
      message_ = nullptr;
    }
    if (arena != nullptr) {
      arena_ = arena;
    }

    unparsed_bytes_.assign(payload, static_cast<size_t>(size));
    state_ = UNPARSED;
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
    state_ = UNPARSED;

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
    state_ = has_value_ ? DIRTY : UNPARSED;
  }

 private:
  enum State { UNPARSED, PARSED, DIRTY };

  void EnsureParsed() const {
    std::lock_guard<std::mutex> lock(mu_);
    EnsureParsedLocked();
  }

  void EnsureParsedNonConst() {
    std::lock_guard<std::mutex> lock(mu_);
    EnsureParsedLocked();
    if (state_ != DIRTY) {
      state_ = DIRTY;
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
      state_ = PARSED;
      return;
    }

    if (!message_->ParseFromArray(unparsed_bytes_.data(),
                                  static_cast<int>(unparsed_bytes_.size()))) {
      message_->Clear();
    }
    state_ = PARSED;
  }

  MessageType* CreateMessage() const {
    if (arena_ != nullptr &&
        std::is_constructible<MessageType, Arena*>::value) {
      return new MessageType(arena_);
    }
    return new MessageType();
  }

  static const char* ReadVarint32(const char* ptr, uint32_t* value) {
    uint32_t result = 0;
    for (int shift = 0; shift < 35; shift += 7) {
      const uint8_t byte = static_cast<uint8_t>(*ptr++);
      result |= static_cast<uint32_t>(byte & 0x7F) << shift;
      if ((byte & 0x80u) == 0) {
        *value = result;
        return ptr;
      }
    }
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
  mutable State state_;
  mutable std::string unparsed_bytes_;
  mutable MessageType* message_;
  Arena* arena_;
  mutable bool has_value_;
};

}  // namespace protoopt

#endif  // PROTOOPT_LAZY_FIELD_H_
