#include "flul/test/expect.hpp"

#include <iostream>
#include <stdexcept>

#include "flul/test/expect_callable.hpp"

using flul::test::AssertionError;
using flul::test::Expect;
using flul::test::ExpectCallable;

namespace {

struct Counts {
    int passed = 0;
    int failed = 0;
};

void RunTest(Counts& counts, const char* name, auto fn) {
    try {
        fn();
        std::cout << "[PASS] " << name << "\n";
        ++counts.passed;
    } catch (const AssertionError& e) {
        std::cout << "[FAIL] " << name << "\n" << e.what() << "\n";
        ++counts.failed;
    } catch (const std::exception& e) {
        std::cout << "[FAIL] " << name << " (unexpected exception: " << e.what() << ")\n";
        ++counts.failed;
    }
}

}  // namespace

int main() {
    Counts counts;

    // ToEqual
    RunTest(counts, "ToEqual passes when equal", [] { Expect(42).ToEqual(42); });
    RunTest(counts, "ToEqual fails when not equal", [] {
        bool threw = false;
        try {
            Expect(1).ToEqual(2);
        } catch (const AssertionError&) {
            threw = true;
        }
        Expect(threw).ToBeTrue();
    });

    // ToNotEqual
    RunTest(counts, "ToNotEqual passes when not equal", [] { Expect(1).ToNotEqual(2); });
    RunTest(counts, "ToNotEqual fails when equal", [] {
        bool threw = false;
        try {
            Expect(1).ToNotEqual(1);
        } catch (const AssertionError&) {
            threw = true;
        }
        Expect(threw).ToBeTrue();
    });

    // ToBeTrue / ToBeFalse
    RunTest(counts, "ToBeTrue passes", [] { Expect(true).ToBeTrue(); });
    RunTest(counts, "ToBeFalse passes", [] { Expect(false).ToBeFalse(); });
    RunTest(counts, "ToBeTrue fails on false", [] {
        bool threw = false;
        try {
            Expect(false).ToBeTrue();
        } catch (const AssertionError&) {
            threw = true;
        }
        Expect(threw).ToBeTrue();
    });

    // Ordering
    RunTest(counts, "ToBeGreaterThan passes", [] { Expect(10).ToBeGreaterThan(5); });
    RunTest(counts, "ToBeLessThan passes", [] { Expect(5).ToBeLessThan(10); });

    // Chaining
    RunTest(counts, "Chaining: ToBeGreaterThan.ToBeLessThan",
            [] { Expect(50).ToBeGreaterThan(0).ToBeLessThan(100); });

    // ExpectCallable
    RunTest(counts, "ToThrow passes when exception matches", [] {
        ExpectCallable([] { throw std::runtime_error("boom"); }).ToThrow<std::runtime_error>();
    });
    RunTest(counts, "ToThrow fails when no exception thrown", [] {
        bool threw = false;
        try {
            ExpectCallable([] {}).ToThrow<std::runtime_error>();
        } catch (const AssertionError&) {
            threw = true;
        }
        Expect(threw).ToBeTrue();
    });
    RunTest(counts, "ToThrow fails on wrong exception type", [] {
        bool threw = false;
        try {
            ExpectCallable([] { throw std::logic_error("wrong"); }).ToThrow<std::runtime_error>();
        } catch (const AssertionError&) {
            threw = true;
        }
        Expect(threw).ToBeTrue();
    });
    RunTest(counts, "ToNotThrow passes", [] { ExpectCallable([] { /* no-op */ }).ToNotThrow(); });
    RunTest(counts, "ToNotThrow fails when exception thrown", [] {
        bool threw = false;
        try {
            ExpectCallable([] { throw std::runtime_error("oops"); }).ToNotThrow();
        } catch (const AssertionError&) {
            threw = true;
        }
        Expect(threw).ToBeTrue();
    });

    std::cout << "\n" << counts.passed << " passed, " << counts.failed << " failed\n";
    return counts.failed > 0 ? 1 : 0;
}
