#include <stdexcept>

#include "flul/test/expect.hpp"
#include "flul/test/expect_callable.hpp"
#include "flul/test/registry.hpp"
#include "flul/test/run.hpp"

using flul::test::Expect;
using flul::test::ExpectCallable;
using flul::test::Registry;
using flul::test::Suite;

// Test methods are registered as void (S::*)() pointers — they cannot be static or const
// even when they don't access instance state, because the Registry::Add template requires
// non-static, non-const member function pointers.
// NOLINTBEGIN(readability-convert-member-functions-to-static,readability-make-member-function-const)

class AssertSuite : public Suite<AssertSuite> {
   public:
    void TestToEqual() {
        Expect(6 * 7).ToEqual(42);
    }
    void TestToNotEqual() {
        Expect(1).ToNotEqual(2);
    }
    void TestToBeTrue() {
        Expect(1 < 2).ToBeTrue();
    }
    void TestToBeFalse() {
        Expect(2 < 1).ToBeFalse();
    }
    void TestToBeGreaterThan() {
        Expect(10).ToBeGreaterThan(5);
    }
    void TestToBeLessThan() {
        Expect(5).ToBeLessThan(10);
    }
    void TestChaining() {
        Expect(50).ToBeGreaterThan(0).ToBeLessThan(100);
    }

    static void Register(Registry& r) {
        AddTests(r, "AssertSuite",
                 {
                     {"TestToEqual", &AssertSuite::TestToEqual},
                     {"TestToNotEqual", &AssertSuite::TestToNotEqual},
                     {"TestToBeTrue", &AssertSuite::TestToBeTrue},
                     {"TestToBeFalse", &AssertSuite::TestToBeFalse},
                     {"TestToBeGreaterThan", &AssertSuite::TestToBeGreaterThan},
                     {"TestToBeLessThan", &AssertSuite::TestToBeLessThan},
                     {"TestChaining", &AssertSuite::TestChaining},
                 });
    }
};

class CallableSuite : public Suite<CallableSuite> {
   public:
    void TestToThrow() {
        ExpectCallable([] { throw std::runtime_error("boom"); }).ToThrow<std::runtime_error>();
    }

    void TestToNotThrow() {
        ExpectCallable([] {}).ToNotThrow();
    }

    static void Register(Registry& r) {
        AddTests(r, "CallableSuite",
                 {
                     {"TestToThrow", &CallableSuite::TestToThrow},
                     {"TestToNotThrow", &CallableSuite::TestToNotThrow},
                 });
    }
};

class FixtureSuite : public Suite<FixtureSuite> {
    int counter_ = 0;

   public:
    void SetUp() override {
        counter_ = 10;
    }
    void TearDown() override {
        counter_ = 0;
    }

    void TestSetUpRan() {
        Expect(counter_).ToEqual(10);
    }
    void TestSetUpRunsPerTest() {
        Expect(counter_).ToEqual(10);
    }

    static void Register(Registry& r) {
        AddTests(r, "FixtureSuite",
                 {
                     {"TestSetUpRan", &FixtureSuite::TestSetUpRan},
                     {"TestSetUpRunsPerTest", &FixtureSuite::TestSetUpRunsPerTest},
                 });
    }
};

// NOLINTEND(readability-convert-member-functions-to-static,readability-make-member-function-const)

auto main(int argc, char* argv[]) -> int {
    Registry registry;
    AssertSuite::Register(registry);
    CallableSuite::Register(registry);
    FixtureSuite::Register(registry);
    return flul::test::Run(argc, argv, registry);
}
