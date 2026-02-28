#ifndef FLUL_TEST_TEST_RESULT_HPP_
#define FLUL_TEST_TEST_RESULT_HPP_

#include <chrono>
#include <functional>
#include <optional>

#include "flul/test/assertion_error.hpp"
#include "flul/test/test_metadata.hpp"

namespace flul::test {

struct TestResult {
    std::reference_wrapper<const TestMetadata> metadata;
    bool passed;
    std::chrono::nanoseconds duration;
    std::optional<AssertionError> error;
};

}  // namespace flul::test

#endif  // FLUL_TEST_TEST_RESULT_HPP_
