#pragma once

#include <concepts>
#include <source_location>
#include <string>

#include "flul/test/assertion_error.hpp"
#include "flul/test/stringify.hpp"

namespace flul::test {

template <typename T>
class Expect {
   public:
    constexpr explicit Expect(T actual, std::source_location loc = std::source_location::current())
        : actual_(std::move(actual)), loc_(loc) {}

    constexpr auto ToEqual(this auto&& self, const T& expected) -> decltype(auto)
        requires std::equality_comparable<T>
    {
        if (self.actual_ != expected) {
            throw AssertionError{Stringify(self.actual_), Stringify(expected), self.loc_};
        }
        return std::forward<decltype(self)>(self);
    }

    constexpr auto ToNotEqual(this auto&& self, const T& unexpected) -> decltype(auto)
        requires std::equality_comparable<T>
    {
        if (self.actual_ == unexpected) {
            throw AssertionError{Stringify(self.actual_), "not " + Stringify(unexpected),
                                 self.loc_};
        }
        return std::forward<decltype(self)>(self);
    }

    constexpr auto ToBeTrue(this auto&& self) -> decltype(auto)
        requires std::convertible_to<T, bool>
    {
        if (!self.actual_) {
            throw AssertionError{Stringify(self.actual_), "true", self.loc_};
        }
        return std::forward<decltype(self)>(self);
    }

    constexpr auto ToBeFalse(this auto&& self) -> decltype(auto)
        requires std::convertible_to<T, bool>
    {
        if (self.actual_) {
            throw AssertionError{Stringify(self.actual_), "false", self.loc_};
        }
        return std::forward<decltype(self)>(self);
    }

    constexpr auto ToBeGreaterThan(this auto&& self, const T& bound) -> decltype(auto)
        requires std::totally_ordered<T>
    {
        if (self.actual_ <= bound) {
            throw AssertionError{Stringify(self.actual_), "greater than " + Stringify(bound),
                                 self.loc_};
        }
        return std::forward<decltype(self)>(self);
    }

    constexpr auto ToBeLessThan(this auto&& self, const T& bound) -> decltype(auto)
        requires std::totally_ordered<T>
    {
        if (self.actual_ >= bound) {
            throw AssertionError{Stringify(self.actual_), "less than " + Stringify(bound),
                                 self.loc_};
        }
        return std::forward<decltype(self)>(self);
    }

   private:
    T actual_;
    std::source_location loc_;
};

}  // namespace flul::test
