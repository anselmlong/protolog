#!/usr/bin/env python3
"""protoc-gen-cpp-opt: Optimized C++ protobuf code generator."""

import sys
import traceback
from google.protobuf.compiler import plugin_pb2

try:
    from compiler.generator.cpp_generator import CppGenerator
except ImportError:
    from generator.cpp_generator import CppGenerator


def main():
    try:
        data = sys.stdin.buffer.read()
        if not data:
            raise ValueError("No input data received from protoc")

        request = plugin_pb2.CodeGeneratorRequest()
        try:
            request.ParseFromString(data)
        except Exception as e:
            raise ValueError(f"Failed to parse CodeGeneratorRequest: {e}")

        response = plugin_pb2.CodeGeneratorResponse()

        for proto_file in request.proto_file:
            if proto_file.name not in request.file_to_generate:
                continue

            try:
                generator = CppGenerator(proto_file, request.parameter)

                header_file = response.file.add()
                header_file.name = proto_file.name.replace(".proto", ".pb.h")
                header_file.content = generator.generate_header()

                source_file = response.file.add()
                source_file.name = proto_file.name.replace(".proto", ".pb.cc")
                source_file.content = generator.generate_source()

            except Exception as e:
                response.error = (
                    f"Error generating code for {proto_file.name}: {str(e)}"
                )
                sys.stderr.write(f"Error: {response.error}\n")
                sys.stderr.write(traceback.format_exc())
                sys.stdout.buffer.write(response.SerializeToString())
                sys.exit(1)

        sys.stdout.buffer.write(response.SerializeToString())

    except Exception as e:
        response = plugin_pb2.CodeGeneratorResponse()
        response.error = f"Fatal error: {str(e)}"
        sys.stderr.write(f"Fatal error: {str(e)}\n")
        sys.stderr.write(traceback.format_exc())
        sys.stdout.buffer.write(response.SerializeToString())
        sys.exit(1)


if __name__ == "__main__":
    main()
