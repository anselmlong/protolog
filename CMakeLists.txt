cmake_minimum_required(VERSION 3.14)
project(cpp-proto-optimizer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options
option(BUILD_TESTING "Build tests" ON)
option(PROTOOPT_INSTALL "Generate install target" ON)

# Find required packages
find_package(Protobuf REQUIRED)

# Find Abseil (required for absl::strings)
find_package(absl REQUIRED)

# Runtime library
add_library(protoopt_runtime STATIC
    runtime/src/protoopt/arena_string.cc
)

# Add library alias for consistent naming
add_library(cpp-proto-optimizer::protoopt_runtime ALIAS protoopt_runtime)

target_include_directories(protoopt_runtime
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/runtime/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${Protobuf_INCLUDE_DIRS}
)

target_link_libraries(protoopt_runtime
    PUBLIC
        protobuf::libprotobuf
        absl::strings
)

# Set library properties
set_target_properties(protoopt_runtime PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    POSITION_INDEPENDENT_CODE ON
)

# Installation
if(PROTOOPT_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    # Install Python plugin
    install(PROGRAMS compiler/plugin.py
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        RENAME protoc-gen-cpp-opt
    )

    # Install runtime headers
    install(DIRECTORY runtime/include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h"
    )

    # Install runtime library
    install(TARGETS protoopt_runtime
        EXPORT cpp-proto-optimizer-targets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    # Export targets
    install(EXPORT cpp-proto-optimizer-targets
        FILE cpp-proto-optimizer-targets.cmake
        NAMESPACE cpp-proto-optimizer::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cpp-proto-optimizer
    )

    # Create package config file
    configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/cpp-proto-optimizer-config.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/cpp-proto-optimizer-config.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cpp-proto-optimizer
    )

    # Create package version file
    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/cpp-proto-optimizer-config-version.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    # Install package config files
    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/cpp-proto-optimizer-config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/cpp-proto-optimizer-config-version.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cpp-proto-optimizer
    )
endif()

# Enable testing
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

# Print configuration summary
message(STATUS "")
message(STATUS "cpp-proto-optimizer Configuration Summary:")
message(STATUS "  Version:           ${PROJECT_VERSION}")
message(STATUS "  C++ Standard:      ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type:        ${CMAKE_BUILD_TYPE}")
message(STATUS "  Install Prefix:    ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  Build Testing:     ${BUILD_TESTING}")
message(STATUS "  Install Target:    ${PROTOOPT_INSTALL}")
message(STATUS "  Protobuf Version:  ${Protobuf_VERSION}")
message(STATUS "")
