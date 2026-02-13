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
include(cmake/FlulTest.cmake)

enable_testing()

# Framework (header-only interface library)
add_library(flul-test INTERFACE)
target_include_directories(flul-test INTERFACE include)

# Assertion subsystem bootstrap test (standalone, no framework runner)
add_executable(expect_test test/expect_test.cpp)
target_link_libraries(expect_test PRIVATE flul-test)
set_project_warnings(expect_test)

# Framework integration test
add_executable(runner_test test/runner_test.cpp)
target_link_libraries(runner_test PRIVATE flul-test)
set_project_warnings(runner_test)
flul_test_discover(runner_test)
