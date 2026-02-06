"""Field-level C++ code generators."""


class FieldType:
    TYPE_DOUBLE = 1
    TYPE_FLOAT = 2
    TYPE_INT64 = 3
    TYPE_UINT64 = 4
    TYPE_INT32 = 5
    TYPE_FIXED64 = 6
    TYPE_FIXED32 = 7
    TYPE_BOOL = 8
    TYPE_STRING = 9
    TYPE_MESSAGE = 11
    TYPE_UINT32 = 13
    TYPE_SFIXED32 = 15
    TYPE_SFIXED64 = 16
    TYPE_SINT32 = 17
    TYPE_SINT64 = 18


class FieldGenerator:
    def __init__(self, field):
        self.field = field
        self.name = field.name
        self.type = field.type
        self.number = field.number

    def generate_declaration(self):
        """Generate field declaration in class."""
        raise NotImplementedError

    def generate_accessor_declarations(self):
        """Generate getter/setter declarations."""
        raise NotImplementedError

    def generate_accessor_definitions(self, class_name):
        """Generate getter/setter definitions."""
        raise NotImplementedError

    def generate_parse_code(self):
        """Generate parsing code for this field."""
        raise NotImplementedError

    def generate_serialize_code(self):
        """Generate serialization code."""
        raise NotImplementedError

    def generate_clear_code(self):
        """Generate field clearing code."""
        raise NotImplementedError


class PrimitiveFieldGenerator(FieldGenerator):
    _CPP_TYPE_MAP = {
        FieldType.TYPE_DOUBLE: "double",
        FieldType.TYPE_FLOAT: "float",
        FieldType.TYPE_INT64: "int64_t",
        FieldType.TYPE_UINT64: "uint64_t",
        FieldType.TYPE_INT32: "int32_t",
        FieldType.TYPE_FIXED64: "uint64_t",
        FieldType.TYPE_FIXED32: "uint32_t",
        FieldType.TYPE_BOOL: "bool",
        FieldType.TYPE_UINT32: "uint32_t",
        FieldType.TYPE_SFIXED32: "int32_t",
        FieldType.TYPE_SFIXED64: "int64_t",
        FieldType.TYPE_SINT32: "int32_t",
        FieldType.TYPE_SINT64: "int64_t",
    }

    _DEFAULT_LITERAL_MAP = {
        FieldType.TYPE_DOUBLE: "0.0",
        FieldType.TYPE_FLOAT: "0.0f",
        FieldType.TYPE_BOOL: "false",
    }

    _WIRE_WRITE_MAP = {
        FieldType.TYPE_DOUBLE: "WriteDouble",
        FieldType.TYPE_FLOAT: "WriteFloat",
        FieldType.TYPE_INT64: "WriteInt64",
        FieldType.TYPE_UINT64: "WriteUInt64",
        FieldType.TYPE_INT32: "WriteInt32",
        FieldType.TYPE_FIXED64: "WriteFixed64",
        FieldType.TYPE_FIXED32: "WriteFixed32",
        FieldType.TYPE_BOOL: "WriteBool",
        FieldType.TYPE_UINT32: "WriteUInt32",
        FieldType.TYPE_SFIXED32: "WriteSFixed32",
        FieldType.TYPE_SFIXED64: "WriteSFixed64",
        FieldType.TYPE_SINT32: "WriteSInt32",
        FieldType.TYPE_SINT64: "WriteSInt64",
    }

    _PARSE_METHOD_MAP = {
        FieldType.TYPE_DOUBLE: "Fixed64Parse",
        FieldType.TYPE_FLOAT: "Fixed32Parse",
        FieldType.TYPE_INT64: "VarintParse",
        FieldType.TYPE_UINT64: "VarintParse",
        FieldType.TYPE_INT32: "VarintParse",
        FieldType.TYPE_FIXED64: "Fixed64Parse",
        FieldType.TYPE_FIXED32: "Fixed32Parse",
        FieldType.TYPE_BOOL: "VarintParse",
        FieldType.TYPE_UINT32: "VarintParse",
        FieldType.TYPE_SFIXED32: "Fixed32Parse",
        FieldType.TYPE_SFIXED64: "Fixed64Parse",
        FieldType.TYPE_SINT32: "VarintParse",
        FieldType.TYPE_SINT64: "VarintParse",
    }

    def _cpp_type(self):
        return self._CPP_TYPE_MAP[self.type]

    def _default_literal(self):
        return self._DEFAULT_LITERAL_MAP.get(self.type, "0")

    def _field_identifier(self):
        return f"{self.name}_"

    def generate_declaration(self):
        return f"{self._cpp_type()} {self._field_identifier()} = {self._default_literal()};"

    def generate_accessor_declarations(self):
        cpp_type = self._cpp_type()
        return "\n".join(
            [
                f"{cpp_type} {self.name}() const;",
                f"void set_{self.name}({cpp_type} value);",
            ]
        )

    def generate_accessor_definitions(self, class_name):
        cpp_type = self._cpp_type()
        field_ident = self._field_identifier()
        return "\n".join(
            [
                f"{cpp_type} {class_name}::{self.name}() const {{",
                f"  return {field_ident};",
                "}",
                "",
                f"void {class_name}::set_{self.name}({cpp_type} value) {{",
                f"  {field_ident} = value;",
                "}",
            ]
        )

    def generate_parse_code(self):
        parse_method = self._PARSE_METHOD_MAP[self.type]
        return "\n".join(
            [
                f"case {self.number}:",
                f"  ptr = {parse_method}(ptr, &{self._field_identifier()});",
                "  break;",
            ]
        )

    def generate_serialize_code(self):
        write_method = self._WIRE_WRITE_MAP[self.type]
        field_ident = self._field_identifier()
        default_literal = self._default_literal()
        return "\n".join(
            [
                f"if ({field_ident} != {default_literal}) {{",
                f"  target = WireFormat::{write_method}({self.number}, {field_ident}, target);",
                "}",
            ]
        )

    def generate_clear_code(self):
        return f"{self._field_identifier()} = {self._default_literal()};"


class StringFieldGenerator(FieldGenerator):
    def _field_identifier(self):
        return f"{self.name}_"

    def generate_declaration(self):
        return f"protoopt::ArenaString {self._field_identifier()};"

    def generate_accessor_declarations(self):
        return "\n".join(
            [
                f"absl::string_view {self.name}() const;",
                f"void set_{self.name}(absl::string_view value);",
                f"std::string* mutable_{self.name}();",
            ]
        )

    def generate_accessor_definitions(self, class_name):
        field_ident = self._field_identifier()
        return "\n".join(
            [
                f"absl::string_view {class_name}::{self.name}() const {{",
                f"  return {field_ident}.Get();",
                "}",
                "",
                f"void {class_name}::set_{self.name}(absl::string_view value) {{",
                f"  {field_ident}.Set(value, arena_);",
                "}",
                "",
                f"std::string* {class_name}::mutable_{self.name}() {{",
                f"  return {field_ident}.Mutable(arena_);",
                "}",
            ]
        )

    def generate_parse_code(self):
        return "\n".join(
            [
                f"case {self.number}:",
                f"  ptr = ReadString(ptr, &{self._field_identifier()}, arena);",
                "  break;",
            ]
        )

    def generate_serialize_code(self):
        field_ident = self._field_identifier()
        return "\n".join(
            [
                f"if (!{field_ident}.IsDefault()) {{",
                f"  target = WireFormat::WriteString({self.number}, {field_ident}.Get(), target);",
                "}",
            ]
        )

    def generate_clear_code(self):
        return f"{self._field_identifier()}.Clear();"


class MessageFieldGenerator(FieldGenerator):
    def _field_identifier(self):
        return f"{self.name}_"

    def _message_cpp_type(self):
        return self.field.type_name.lstrip(".").replace(".", "::")

    def generate_declaration(self):
        return f"{self._message_cpp_type()}* {self._field_identifier()} = nullptr;"

    def generate_accessor_declarations(self):
        message_type = self._message_cpp_type()
        return "\n".join(
            [
                f"bool has_{self.name}() const;",
                f"const {message_type}& {self.name}() const;",
                f"{message_type}* mutable_{self.name}();",
            ]
        )

    def generate_accessor_definitions(self, class_name):
        message_type = self._message_cpp_type()
        field_ident = self._field_identifier()
        return "\n".join(
            [
                f"bool {class_name}::has_{self.name}() const {{",
                f"  return {field_ident} != nullptr;",
                "}",
                "",
                f"const {message_type}& {class_name}::{self.name}() const {{",
                f"  return *{field_ident};",
                "}",
                "",
                f"{message_type}* {class_name}::mutable_{self.name}() {{",
                f"  if ({field_ident} == nullptr) {{",
                f"    {field_ident} = ::google::protobuf::Arena::CreateMessage<{message_type}>(arena_);",
                "  }",
                f"  return {field_ident};",
                "}",
            ]
        )

    def generate_parse_code(self):
        return "\n".join(
            [
                f"case {self.number}:",
                f"  ptr = ReadMessage(ptr, mutable_{self.name}(), ctx);",
                "  break;",
            ]
        )

    def generate_serialize_code(self):
        field_ident = self._field_identifier()
        return "\n".join(
            [
                f"if ({field_ident} != nullptr) {{",
                f"  target = WireFormat::WriteMessage({self.number}, *{field_ident}, target, stream);",
                "}",
            ]
        )

    def generate_clear_code(self):
        field_ident = self._field_identifier()
        return "\n".join(
            [
                f"if ({field_ident} != nullptr) {{",
                f"  {field_ident}->Clear();",
                "}",
            ]
        )


class LazyMessageFieldGenerator(FieldGenerator):
    def _field_identifier(self):
        return f"{self.name}_"

    def _message_cpp_type(self):
        return self.field.type_name.lstrip(".").replace(".", "::")

    def generate_declaration(self):
        return f"protoopt::LazyField<{self._message_cpp_type()}> {self._field_identifier()};"

    def generate_accessor_declarations(self):
        message_type = self._message_cpp_type()
        return "\n".join(
            [
                f"const {message_type}& {self.name}() const;",
                f"{message_type}* mutable_{self.name}();",
            ]
        )

    def generate_accessor_definitions(self, class_name):
        message_type = self._message_cpp_type()
        field_ident = self._field_identifier()
        return "\n".join(
            [
                f"const {message_type}& {class_name}::{self.name}() const {{",
                f"  return {field_ident}.Get();",
                "}",
                "",
                f"{message_type}* {class_name}::mutable_{self.name}() {{",
                f"  return {field_ident}.Mutable();",
                "}",
            ]
        )

    def generate_parse_code(self):
        return "\n".join(
            [
                f"case {self.number}:",
                f"  ptr = {self._field_identifier()}.ParseFrom(ptr, ctx, arena);",
                "  break;",
            ]
        )

    def generate_serialize_code(self):
        return f"target = {self._field_identifier()}.Serialize(target, stream);"

    def generate_clear_code(self):
        return f"{self._field_identifier()}.Clear();"


def create_field_generator(field):
    if field.type == FieldType.TYPE_STRING:
        return StringFieldGenerator(field)

    if field.type == FieldType.TYPE_MESSAGE:
        is_lazy = getattr(field.options, "lazy", False)
        if is_lazy:
            return LazyMessageFieldGenerator(field)
        return MessageFieldGenerator(field)

    return PrimitiveFieldGenerator(field)
