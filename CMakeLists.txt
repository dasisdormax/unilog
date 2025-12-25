cmake_minimum_required(VERSION 3.10)
project(unilog VERSION 1.0.0 LANGUAGES C)

# Set C11 standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Library source files
set(UNILOG_SOURCES
    src/unilog.c
)

set(UNILOG_HEADERS
    include/unilog/unilog.h
)

# Create static library
add_library(unilog STATIC ${UNILOG_SOURCES} ${UNILOG_HEADERS})

# Include directories
target_include_directories(unilog
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Compiler warnings
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(unilog PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )
endif()

# Examples
option(UNILOG_BUILD_EXAMPLES "Build example programs" ON)
if(UNILOG_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Tests
option(UNILOG_BUILD_TESTS "Build test programs" ON)
if(UNILOG_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Installation
install(TARGETS unilog
    EXPORT unilogTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

install(DIRECTORY include/
    DESTINATION include
)

install(EXPORT unilogTargets
    FILE unilogTargets.cmake
    NAMESPACE unilog::
    DESTINATION lib/cmake/unilog
)

# Generate package config
include(CMakePackageConfigHelpers)
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/unilogConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/unilogConfig.cmake
    INSTALL_DESTINATION lib/cmake/unilog
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/unilogConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/unilogConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/unilogConfigVersion.cmake
    DESTINATION lib/cmake/unilog
)
