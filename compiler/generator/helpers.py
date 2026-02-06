"""Helper utilities for C++ code generation from protobuf descriptors."""

from google.protobuf import descriptor_pb2
import re


def cpp_type(field):
    """Map protobuf field type to C++ type.

    Args:
        field: FieldDescriptorProto from protobuf descriptor

    Returns:
        str: C++ type name
    """
    type_map = {
        descriptor_pb2.FieldDescriptorProto.TYPE_INT32: "int32_t",
        descriptor_pb2.FieldDescriptorProto.TYPE_INT64: "int64_t",
        descriptor_pb2.FieldDescriptorProto.TYPE_UINT32: "uint32_t",
        descriptor_pb2.FieldDescriptorProto.TYPE_UINT64: "uint64_t",
        descriptor_pb2.FieldDescriptorProto.TYPE_SINT32: "int32_t",
        descriptor_pb2.FieldDescriptorProto.TYPE_SINT64: "int64_t",
        descriptor_pb2.FieldDescriptorProto.TYPE_FIXED32: "uint32_t",
        descriptor_pb2.FieldDescriptorProto.TYPE_FIXED64: "uint64_t",
        descriptor_pb2.FieldDescriptorProto.TYPE_SFIXED32: "int32_t",
        descriptor_pb2.FieldDescriptorProto.TYPE_SFIXED64: "int64_t",
        descriptor_pb2.FieldDescriptorProto.TYPE_FLOAT: "float",
        descriptor_pb2.FieldDescriptorProto.TYPE_DOUBLE: "double",
        descriptor_pb2.FieldDescriptorProto.TYPE_BOOL: "bool",
        descriptor_pb2.FieldDescriptorProto.TYPE_STRING: "protoopt::ArenaString",
        descriptor_pb2.FieldDescriptorProto.TYPE_BYTES: "protoopt::ArenaString",
    }

    if field.type in type_map:
        return type_map[field.type]

    if field.type == descriptor_pb2.FieldDescriptorProto.TYPE_MESSAGE:
        # Convert .package.MessageName to package::MessageName*
        type_name = field.type_name.lstrip(".")
        return type_name.replace(".", "::") + "*"

    if field.type == descriptor_pb2.FieldDescriptorProto.TYPE_ENUM:
        # Convert .package.EnumName to package::EnumName
        type_name = field.type_name.lstrip(".")
        return type_name.replace(".", "::")

    return "unknown"


def wire_type_name(wire_type):
    """Get wire type constant name.

    Args:
        wire_type: Wire type constant from protobuf

    Returns:
        str: Wire type constant name (e.g., 'kVarint')
    """
    wire_type_map = {
        0: "kVarint",  # WIRETYPE_VARINT
        1: "kFixed64",  # WIRETYPE_FIXED64
        2: "kLengthDelimited",  # WIRETYPE_LENGTH_DELIMITED
        5: "kFixed32",  # WIRETYPE_FIXED32
    }
    return wire_type_map.get(wire_type, "kUnknown")


def is_lazy_field(field):
    """Check if field has [lazy=true] option.

    Args:
        field: FieldDescriptorProto from protobuf descriptor

    Returns:
        bool: True if field is marked as lazy
    """
    if not hasattr(field, "options") or not field.options:
        return False

    # Check if the lazy option is set
    if hasattr(field.options, "lazy"):
        return field.options.lazy

    # Fallback: parse string representation for custom options
    options_str = str(field.options)
    if "lazy" in options_str.lower():
        # Look for patterns like "lazy: true" or "[lazy=true]"
        return "true" in options_str.lower()

    return False


def is_string_field(field):
    """Check if field is string or bytes type.

    Args:
        field: FieldDescriptorProto from protobuf descriptor

    Returns:
        bool: True if field is TYPE_STRING or TYPE_BYTES
    """
    return field.type in (
        descriptor_pb2.FieldDescriptorProto.TYPE_STRING,
        descriptor_pb2.FieldDescriptorProto.TYPE_BYTES,
    )


def is_message_field(field):
    """Check if field is message type.

    Args:
        field: FieldDescriptorProto from protobuf descriptor

    Returns:
        bool: True if field is TYPE_MESSAGE
    """
    return field.type == descriptor_pb2.FieldDescriptorProto.TYPE_MESSAGE


def field_name(field):
    """Convert proto field name to C++ member name.

    Args:
        field: FieldDescriptorProto from protobuf descriptor

    Returns:
        str: C++ member variable name (with trailing underscore)
    """
    # Proto field names are typically snake_case
    # C++ member names use snake_case with trailing underscore
    return field.name + "_"


def class_name(message):
    """Get C++ class name from message descriptor.

    Args:
        message: DescriptorProto from protobuf descriptor

    Returns:
        str: C++ class name
    """
    return message.name


def namespace(proto_file):
    """Get C++ namespace from proto package.

    Args:
        proto_file: FileDescriptorProto from protobuf descriptor

    Returns:
        list: List of namespace components (e.g., ['foo', 'bar'] for package foo.bar)
    """
    if not hasattr(proto_file, "package") or not proto_file.package:
        return []

    # Split package name by dots
    return proto_file.package.split(".")


def header_guard(file_name):
    """Generate include guard macro name.

    Args:
        file_name: Proto file name (e.g., 'foo/bar.proto')

    Returns:
        str: Include guard macro (e.g., 'FOO_BAR_PROTO_H_')
    """
    # Remove .proto extension and convert to uppercase
    # Replace path separators and dots with underscores
    guard = file_name.replace(".proto", "")
    guard = re.sub(r"[/\.\-]", "_", guard)
    guard = guard.upper()
    return guard + "_PROTO_H_"


def includes_for_message(message):
    """Get required #include directives for a message.

    Args:
        message: DescriptorProto from protobuf descriptor

    Returns:
        list: List of include directives needed for this message
    """
    includes = set()

    # Always need base runtime includes
    includes.add('#include "runtime/arena.h"')
    includes.add('#include "runtime/message.h"')

    # Check fields for additional includes
    for field in message.field:
        if is_string_field(field):
            includes.add('#include "runtime/arena_string.h"')

        if is_message_field(field):
            # Convert .package.MessageName to package/message_name.pb.h
            type_name = field.type_name.lstrip(".")
            # Split by dots and convert to path
            parts = type_name.split(".")
            if len(parts) > 1:
                # Last part is the message name, rest is package
                package_path = "/".join(parts[:-1])
                message_name = parts[-1]
                # Convert CamelCase to snake_case for file name
                file_name = re.sub(r"(?<!^)(?=[A-Z])", "_", message_name).lower()
                includes.add(f'#include "{package_path}/{file_name}.pb.h"')

        if field.type == descriptor_pb2.FieldDescriptorProto.TYPE_ENUM:
            # Similar handling for enums
            type_name = field.type_name.lstrip(".")
            parts = type_name.split(".")
            if len(parts) > 1:
                package_path = "/".join(parts[:-1])
                enum_name = parts[-1]
                file_name = re.sub(r"(?<!^)(?=[A-Z])", "_", enum_name).lower()
                includes.add(f'#include "{package_path}/{file_name}.pb.h"')

        # Check for repeated fields
        if field.label == descriptor_pb2.FieldDescriptorProto.LABEL_REPEATED:
            includes.add("#include <vector>")

    # Check for nested messages or enums
    if message.nested_type:
        # Nested messages might need additional includes
        pass

    if message.enum_type:
        # Enums are typically defined inline, no extra includes needed
        pass

    return sorted(list(includes))
