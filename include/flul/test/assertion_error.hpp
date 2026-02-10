#pragma once

#include <exception>
#include <format>
#include <source_location>
#include <string>

namespace flul::test {

class AssertionError : public std::exception {
   public:
    // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes): intentional public
    // fields — Runner/output layer accesses these directly without getters.
    std::string actual;    // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)
    std::string expected;  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)
    std::source_location
        location;  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)

    constexpr AssertionError(std::string actual_val, std::string expected_val,
                             std::source_location loc)
        : actual(std::move(actual_val)),
          expected(std::move(expected_val)),
          location(loc),
          what_(std::format("{}:{}: assertion failed\n  expected: {}\n    actual: {}",
                            location.file_name(), location.line(), expected, actual)) {}

    [[nodiscard]] auto what() const noexcept -> const char* override {
        return what_.c_str();
    }

   private:
    std::string what_;
};

}  // namespace flul::test
