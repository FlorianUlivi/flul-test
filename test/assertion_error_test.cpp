#include "flul/test/assertion_error.hpp"

#include <source_location>
#include <string>

#include "flul/test/expect.hpp"
#include "flul/test/registry.hpp"

using flul::test::AssertionError;
using flul::test::Expect;
using flul::test::Registry;
using flul::test::Suite;

// NOLINTBEGIN(readability-convert-member-functions-to-static,readability-make-member-function-const)

class AssertionErrorSuite : public Suite<AssertionErrorSuite> {
   public:
    void TestWhatFormat() {
        auto loc = std::source_location::current();
        AssertionError err("actual_val", "expected_val", loc);  // NOLINT(misc-const-correctness)

        std::string msg = err.what();  // NOLINT(misc-const-correctness)
        Expect(msg.contains("assertion failed")).ToBeTrue();
        Expect(msg.contains("expected: expected_val")).ToBeTrue();
        Expect(msg.contains("actual: actual_val")).ToBeTrue();
    }

    void TestPublicFields() {
        auto loc = std::source_location::current();
        AssertionError err("got", "want", loc);  // NOLINT(misc-const-correctness)

        Expect(err.actual).ToEqual(std::string("got"));
        Expect(err.expected).ToEqual(std::string("want"));
        Expect(err.location.line()).ToEqual(loc.line());
    }

    static void Register(Registry& r) {
        AddTests(r, "AssertionErrorSuite",
                 {
                     {"TestWhatFormat", &AssertionErrorSuite::TestWhatFormat},
                     {"TestPublicFields", &AssertionErrorSuite::TestPublicFields},
                 });
    }
};

// NOLINTEND(readability-convert-member-functions-to-static,readability-make-member-function-const)

namespace assertion_error_test {
void Register(Registry& r) {  // NOLINT(misc-use-internal-linkage)
    AssertionErrorSuite::Register(r);
}
}  // namespace assertion_error_test
