#include "flul/test/expect_callable.hpp"

#include <stdexcept>

#include "flul/test/assertion_error.hpp"
#include "flul/test/registry.hpp"

using flul::test::AssertionError;
using flul::test::ExpectCallable;
using flul::test::Registry;
using flul::test::Suite;

// NOLINTBEGIN(readability-convert-member-functions-to-static,readability-make-member-function-const)

class ExpectCallableSuite : public Suite<ExpectCallableSuite> {
   public:
    void TestToThrowPass() {
        ExpectCallable([] { throw std::runtime_error("boom"); }).ToThrow<std::runtime_error>();
    }

    void TestToThrowNoException() {
        ExpectCallable([] {
            ExpectCallable([] {}).ToThrow<std::runtime_error>();
        }).ToThrow<AssertionError>();
    }

    void TestToThrowWrongException() {
        ExpectCallable([] {
            ExpectCallable([] { throw std::logic_error("wrong"); }).ToThrow<std::runtime_error>();
        }).ToThrow<AssertionError>();
    }

    void TestToNotThrowPass() {
        ExpectCallable([] {}).ToNotThrow();
    }

    void TestToNotThrowStdException() {
        ExpectCallable([] {
            ExpectCallable([] { throw std::runtime_error("oops"); }).ToNotThrow();
        }).ToThrow<AssertionError>();
    }

    void TestToNotThrowUnknownException() {
        ExpectCallable([] {
            ExpectCallable([] { throw 42; }).ToNotThrow();  // NOLINT(hicpp-exception-baseclass)
        }).ToThrow<AssertionError>();
    }

    static void Register(Registry& r) {
        AddTests(
            r, "ExpectCallableSuite",
            {
                {"TestToThrowPass", &ExpectCallableSuite::TestToThrowPass},
                {"TestToThrowNoException", &ExpectCallableSuite::TestToThrowNoException},
                {"TestToThrowWrongException", &ExpectCallableSuite::TestToThrowWrongException},
                {"TestToNotThrowPass", &ExpectCallableSuite::TestToNotThrowPass},
                {"TestToNotThrowStdException", &ExpectCallableSuite::TestToNotThrowStdException},
                {"TestToNotThrowUnknownException",
                 &ExpectCallableSuite::TestToNotThrowUnknownException},
            });
    }
};

// NOLINTEND(readability-convert-member-functions-to-static,readability-make-member-function-const)

namespace expect_callable_test {
void Register(Registry& r) {  // NOLINT(misc-use-internal-linkage)
    ExpectCallableSuite::Register(r);
}
}  // namespace expect_callable_test
