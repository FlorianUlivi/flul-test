#ifndef FLUL_TEST_ASSERTION_ERROR_HPP_
#define FLUL_TEST_ASSERTION_ERROR_HPP_

#include <exception>
#include <format>
#include <source_location>
#include <string>

namespace flul::test {

class AssertionError : public std::exception {
    std::string what_;

   public:
    // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes): intentional public
    // fields — Runner/output layer accesses these directly without getters.
    std::string actual;    // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)
    std::string expected;  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)
    std::source_location
        location;  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)

    constexpr AssertionError(std::string actual_val, std::string expected_val,
                             std::source_location loc)
        : what_(std::format("{}:{}: assertion failed\n  expected: {}\n    actual: {}",
                            loc.file_name(), loc.line(), expected_val, actual_val)),
          actual(std::move(actual_val)),
          expected(std::move(expected_val)),
          location(loc) {}

    [[nodiscard]] auto what() const noexcept -> const char* override {
        return what_.c_str();
    }
};

}  // namespace flul::test

#endif  // FLUL_TEST_ASSERTION_ERROR_HPP_
