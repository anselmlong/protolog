# Find GoogleTest
find_package(GTest REQUIRED)

# Test executable
add_executable(protoopt_tests
    test_main.cc
    arena_string_test.cc
    lazy_field_test.cc
    skip_unknown_test.cc
)

target_link_libraries(protoopt_tests
    PRIVATE
        protoopt_runtime
        protobuf::libprotobuf
        GTest::gtest_main
)

# Include directories
target_include_directories(protoopt_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/include
    ${CMAKE_CURRENT_BINARY_DIR}  # For generated proto headers
)

# Set test properties
set_target_properties(protoopt_tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Add tests to CTest
include(GoogleTest)
gtest_discover_tests(protoopt_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    PROPERTIES
        TIMEOUT 30
)

# Function to generate optimized proto files
function(generate_proto_opt PROTO_FILE)
    get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
    get_filename_component(PROTO_DIR ${PROTO_FILE} DIRECTORY)
    get_filename_component(PROTO_ABS ${PROTO_FILE} ABSOLUTE)
    
    set(PROTO_HDR ${CMAKE_CURRENT_BINARY_DIR}/${PROTO_NAME}.pb.h)
    set(PROTO_SRC ${CMAKE_CURRENT_BINARY_DIR}/${PROTO_NAME}.pb.cc)
    
    # Get the plugin path
    set(PLUGIN_PATH ${CMAKE_SOURCE_DIR}/compiler/plugin.py)
    
    add_custom_command(
        OUTPUT ${PROTO_HDR} ${PROTO_SRC}
        COMMAND ${Protobuf_PROTOC_EXECUTABLE}
        ARGS --plugin=protoc-gen-cpp-opt=${PLUGIN_PATH}
             --cpp-opt_out=${CMAKE_CURRENT_BINARY_DIR}
             -I${PROTO_DIR}
             ${PROTO_ABS}
        DEPENDS ${PROTO_ABS} ${PLUGIN_PATH}
        COMMENT "Generating optimized C++ from ${PROTO_NAME}.proto"
        VERBATIM
    )
    
    # Add generated sources to test target
    target_sources(protoopt_tests PRIVATE ${PROTO_SRC})
    
    # Set source file properties
    set_source_files_properties(${PROTO_SRC} ${PROTO_HDR}
        PROPERTIES GENERATED TRUE
    )
endfunction()

# Generate test protos from examples directory
file(GLOB TEST_PROTOS "${CMAKE_SOURCE_DIR}/examples/*.proto")
if(TEST_PROTOS)
    foreach(proto ${TEST_PROTOS})
        generate_proto_opt(${proto})
    endforeach()
    message(STATUS "Found ${CMAKE_MATCH_COUNT} test proto files")
else()
    message(WARNING "No .proto files found in examples/ directory")
endif()

# Alternative: Generate specific test protos if they exist
set(SPECIFIC_TEST_PROTOS
    "${CMAKE_SOURCE_DIR}/examples/test.proto"
    "${CMAKE_SOURCE_DIR}/examples/benchmark.proto"
)

foreach(proto ${SPECIFIC_TEST_PROTOS})
    if(EXISTS ${proto})
        generate_proto_opt(${proto})
    endif()
endforeach()
