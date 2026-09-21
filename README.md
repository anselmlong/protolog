# cpp-proto-optimizer

An experimental C++ Protocol Buffers code generator exploring three optimizations:

- **[lazy = true]**: Deferred parsing for submessage fields
- **Arena-allocated strings with string_view**: Zero-copy string access  
- **Unknown field skipping**: Skip storage of unknown fields for terminal consumers

## Why?

Google's internal protobuf implementation includes performance features that never made it to open-source:
- ArenaStringPtr with true arena allocation
- Native [lazy=true] support  
- SIMD varint decoding
- Cord integration

This project explores these techniques in a custom generator and runtime. It is a prototype, not a drop-in replacement for the official C++ generator.

## Performance Validation

The repository includes `examples/benchmark.proto`, but does not yet include a reproducible benchmark harness or published measurements. Performance benefits are hypotheses to measure against the official generator using the same schemas, compiler, protobuf version, allocator, and hardware. No speedup or memory-reduction guarantee is currently established.

## Features

### 1. Lazy Field Parsing

Defer parsing of submessage fields until first access:

```protobuf
message Request {
  string id = 1;
  LargePayload data = 2 [lazy = true];  // Stored as raw bytes
}
```

**Benefits:**
- Skip parsing of large payloads that are never accessed
- Zero-copy message forwarding
- Reduced memory pressure for filtering applications

### 2. Arena-Allocated Strings

Custom string storage optimized for arena allocation:

```cpp
// Generated code uses ArenaString instead of std::string
protoopt::ArenaString name_;

// Zero-copy string_view access
absl::string_view name() const { return name_.Get(); }
```

**Benefits:**
- Faster allocation via arena bump-pointer
- Zero-copy string_view accessors
- Reduced heap fragmentation

### 3. Unknown Field Skipping

Skip storage of unknown fields entirely:

```cpp
// Standard protobuf: stores unknown fields in UnknownFieldSet
// This generator: skips unknown fields with no allocation
```

**Benefits:**
- Avoids the work of retaining unknown fields
- Eliminates UnknownFieldSet allocation
- Ideal for terminal message consumers

## Building

### Prerequisites

- CMake 3.14+
- C++17 compiler (GCC 8+, Clang 7+, MSVC 2019+)
- Protocol Buffers 3.12+
- Python 3.7+ (for the plugin)
- Abseil library

### Build Instructions

```bash
# Clone the repository
cd cpp-proto-optimizer

# Create build directory
mkdir build && cd build

# Configure
cmake ..

# Build
make -j$(nproc)

# Run tests
make test
```

## Usage

### 1. Generate Optimized Code

```bash
# Use the plugin with protoc
protoc --plugin=protoc-gen-cpp-opt=./compiler/plugin.py \
       --cpp-opt_out=. \
       my_message.proto

# Generates: my_message.pb.h, my_message.pb.cc
```

### 2. Link Runtime Library

```cmake
find_package(cpp-proto-optimizer REQUIRED)

target_link_libraries(my_target
    cpp-proto-optimizer::protoopt_runtime
)
```

### 3. Use Generated Messages

```cpp
#include "my_message.pb.h"
#include "google/protobuf/arena.h"

// Use with arena for best performance
google::protobuf::Arena arena;
MyMessage* msg = google::protobuf::Arena::Create<MyMessage>(&arena);

// String fields use string_view - zero copy
msg->set_name("example");
absl::string_view name = msg->name();  // No allocation

// Lazy fields parse on first access
if (msg->has_payload()) {
  const auto& payload = msg->payload();  // Parse happens here
  // ...
}
```

## Project Structure

```
cpp-proto-optimizer/
├── compiler/          # Python protoc plugin
│   ├── plugin.py      # Entry point
│   └── generator/     # Code generation modules
├── runtime/           # C++ support library
│   ├── include/       # ArenaString, LazyField, etc.
│   └── src/
├── examples/          # Example .proto files
└── tests/             # Unit tests
```

## Architecture

### Code Generation Pipeline

1. **Parse**: protoc parses .proto → FileDescriptorProto
2. **Generate**: Plugin receives descriptor, generates C++
3. **Output**: .pb.h/.pb.cc files using optimized runtime

### Runtime Components

- **ArenaString**: Tagged pointer string storage with arena support
- **LazyField<T>**: Template for deferred message parsing
- **UnknownFieldSkipper**: Zero-allocation field skipping

## Comparison with Standard Protobuf

| Feature | Standard | cpp-proto-optimizer |
|---------|----------|---------------------|
| Lazy parsing | ❌ | ✅ [lazy=true] |
| Arena strings | Partial | ✅ Full arena allocation |
| String views | ❌ | ✅ Zero-copy accessors |
| Unknown field skip | ❌ | ✅ Compile-time option |
| Reflection | Available | Not implemented: metadata pointers are null |
| Binary compat | Reference implementation | Intended wire compatibility; conformance not established |

## Limitations

1. **Lazy fields**: Only works for singular submessage fields
2. **Unknown field skip**: Cannot forward messages after skipping
3. **Reflection**: `GetMetadata()` returns null descriptor and reflection pointers. Do not pass generated messages to reflection-dependent APIs (including descriptor inspection, generic JSON/TextFormat conversion, or reflection-based utilities). Merely inheriting from `google::protobuf::Message` does not establish compatibility with those APIs.
4. **Determinism**: Lazy fields may affect byte-for-byte serialization
5. **API and version support**: The generator uses protobuf implementation APIs. The documented minimum version is not a tested compatibility matrix. Validate compilation and round-trip behavior on the exact protobuf version and schemas used by your application before adopting it.

## Contributing

Contributions welcome! Areas for improvement:
- SIMD varint decoding
- Cord integration  
- Descriptor registration and reflection support
- Additional language bindings

## License

MIT License - See LICENSE file

## Acknowledgments

This project implements optimizations described in Google protobuf internals research and the Protocol Buffers wire format specification.
