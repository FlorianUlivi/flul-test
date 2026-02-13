#include "flul/test/expect.hpp"

#include "flul/test/assertion_error.hpp"
#include "flul/test/expect_callable.hpp"
#include "flul/test/registry.hpp"

using flul::test::AssertionError;
using flul::test::Expect;
using flul::test::ExpectCallable;
using flul::test::Registry;
using flul::test::Suite;

// NOLINTBEGIN(readability-convert-member-functions-to-static,readability-make-member-function-const)

class ExpectSuite : public Suite<ExpectSuite> {
   public:
    void TestToEqualPass() {
        Expect(42).ToEqual(42);
    }

    void TestToEqualFail() {
        ExpectCallable([] { Expect(1).ToEqual(2); }).ToThrow<AssertionError>();
    }

    void TestToNotEqualPass() {
        Expect(1).ToNotEqual(2);
    }

    void TestToNotEqualFail() {
        ExpectCallable([] { Expect(1).ToNotEqual(1); }).ToThrow<AssertionError>();
    }

    void TestToBeTruePass() {
        Expect(true).ToBeTrue();
    }

    void TestToBeTrueFail() {
        ExpectCallable([] { Expect(false).ToBeTrue(); }).ToThrow<AssertionError>();
    }

    void TestToBeFalsePass() {
        Expect(false).ToBeFalse();
    }

    void TestToBeFalseFail() {
        ExpectCallable([] { Expect(true).ToBeFalse(); }).ToThrow<AssertionError>();
    }

    void TestToBeGreaterThanPass() {
        Expect(10).ToBeGreaterThan(5);
    }

    void TestToBeGreaterThanFail() {
        ExpectCallable([] { Expect(1).ToBeGreaterThan(10); }).ToThrow<AssertionError>();
    }

    void TestToBeLessThanPass() {
        Expect(5).ToBeLessThan(10);
    }

    void TestToBeLessThanFail() {
        ExpectCallable([] { Expect(10).ToBeLessThan(1); }).ToThrow<AssertionError>();
    }

    void TestChaining() {
        Expect(50).ToBeGreaterThan(0).ToBeLessThan(100);
    }

    static void Register(Registry& r) {
        AddTests(r, "ExpectSuite",
                 {
                     {"TestToEqualPass", &ExpectSuite::TestToEqualPass},
                     {"TestToEqualFail", &ExpectSuite::TestToEqualFail},
                     {"TestToNotEqualPass", &ExpectSuite::TestToNotEqualPass},
                     {"TestToNotEqualFail", &ExpectSuite::TestToNotEqualFail},
                     {"TestToBeTruePass", &ExpectSuite::TestToBeTruePass},
                     {"TestToBeTrueFail", &ExpectSuite::TestToBeTrueFail},
                     {"TestToBeFalsePass", &ExpectSuite::TestToBeFalsePass},
                     {"TestToBeFalseFail", &ExpectSuite::TestToBeFalseFail},
                     {"TestToBeGreaterThanPass", &ExpectSuite::TestToBeGreaterThanPass},
                     {"TestToBeGreaterThanFail", &ExpectSuite::TestToBeGreaterThanFail},
                     {"TestToBeLessThanPass", &ExpectSuite::TestToBeLessThanPass},
                     {"TestToBeLessThanFail", &ExpectSuite::TestToBeLessThanFail},
                     {"TestChaining", &ExpectSuite::TestChaining},
                 });
    }
};

// NOLINTEND(readability-convert-member-functions-to-static,readability-make-member-function-const)

namespace expect_test {
void Register(Registry& r) {  // NOLINT(misc-use-internal-linkage)
    ExpectSuite::Register(r);
}
}  // namespace expect_test
