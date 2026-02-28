cmake_minimum_required(VERSION 3.31)

project(flul-test
    VERSION 1.0.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/CompilerWarnings.cmake)
include(cmake/FlulTest.cmake)

if(FLUL_COVERAGE)
    include(cmake/Coverage.cmake)
endif()

enable_testing()

# Framework (header-only interface library)
add_library(flul-test INTERFACE)
target_include_directories(flul-test INTERFACE include)

# Self-test: the framework tests itself
add_executable(self_test
    test/self_test.cpp
    test/assertion_error_test.cpp
    test/stringify_test.cpp
    test/expect_test.cpp
    test/expect_callable_test.cpp
    test/registry_test.cpp
    test/runner_test.cpp
    test/run_test.cpp
    test/fixture_test.cpp
    test/tag_test.cpp
    test/tag_adversarial_test.cpp
    test/tag_dedup_test.cpp
)
target_link_libraries(self_test PRIVATE flul-test)
set_project_warnings(self_test)
flul_test_discover(self_test)

if(FLUL_COVERAGE)
    enable_coverage_for_target(self_test)
endif()
