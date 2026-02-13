#ifndef FLUL_TEST_TEST_RESULT_HPP_
#define FLUL_TEST_TEST_RESULT_HPP_

#include <chrono>
#include <optional>
#include <string_view>

#include "flul/test/assertion_error.hpp"

namespace flul::test {

struct TestResult {
    std::string_view suite_name;
    std::string_view test_name;
    bool passed;
    std::chrono::nanoseconds duration;
    std::optional<AssertionError> error;
};

}  // namespace flul::test

#endif  // FLUL_TEST_TEST_RESULT_HPP_
