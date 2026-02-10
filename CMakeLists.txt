cmake_minimum_required(VERSION 4.0)

project(flul-test
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/CompilerWarnings.cmake)

# Assertion subsystem (header-only interface library)
add_library(flul-test-expect INTERFACE)
target_include_directories(flul-test-expect INTERFACE include)

# Test binary
add_executable(expect_test test/expect_test.cpp)
target_link_libraries(expect_test PRIVATE flul-test-expect)
set_project_warnings(expect_test)
