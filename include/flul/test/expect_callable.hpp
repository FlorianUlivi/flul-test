#pragma once

#include <concepts>
#include <source_location>
#include <typeinfo>

#include "flul/test/assertion_error.hpp"
#include "flul/test/stringify.hpp"

namespace flul::test {

template <std::invocable F>
class ExpectCallable {
   public:
    constexpr explicit ExpectCallable(F callable,
                                      std::source_location loc = std::source_location::current())
        : callable_(std::move(callable)), loc_(loc) {}

    template <typename E>
    auto ToThrow() -> void {
        try {
            callable_();
        } catch (E&) {
            return;
        } catch (...) {
            throw AssertionError{
                "different exception",
                Demangle(typeid(E).name()),
                loc_,
            };
        }
        throw AssertionError{
            "no exception",
            Demangle(typeid(E).name()),
            loc_,
        };
    }

    auto ToNotThrow() -> void {
        try {
            callable_();
        } catch (std::exception& e) {
            throw AssertionError{std::string(e.what()), "no exception", loc_};
        } catch (...) {
            throw AssertionError{"unknown exception", "no exception", loc_};
        }
    }

   private:
    F callable_;
    std::source_location loc_;
};

}  // namespace flul::test
