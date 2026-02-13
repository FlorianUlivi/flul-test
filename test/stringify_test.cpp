#include "flul/test/stringify.hpp"

#include <string>

#include "flul/test/expect.hpp"
#include "flul/test/registry.hpp"

using flul::test::Demangle;
using flul::test::Expect;
using flul::test::Registry;
using flul::test::Stringify;
using flul::test::Suite;

namespace {

struct NonPrintable {};

}  // namespace

// NOLINTBEGIN(readability-convert-member-functions-to-static,readability-make-member-function-const)

class StringifySuite : public Suite<StringifySuite> {
   public:
    void TestFormattable() {
        Expect(Stringify(42)).ToEqual(std::string("42"));
    }

    void TestNonPrintable() {
        NonPrintable val;  // NOLINT(misc-const-correctness)
        Expect(Stringify(val)).ToEqual(std::string("<non-printable>"));
    }

    void TestDemangle() {
        auto result = Demangle(typeid(std::string).name());
        Expect(result.contains("basic_string")).ToBeTrue();
    }

    static void Register(Registry& r) {
        AddTests(r, "StringifySuite",
                 {
                     {"TestFormattable", &StringifySuite::TestFormattable},
                     {"TestNonPrintable", &StringifySuite::TestNonPrintable},
                     {"TestDemangle", &StringifySuite::TestDemangle},
                 });
    }
};

// NOLINTEND(readability-convert-member-functions-to-static,readability-make-member-function-const)

namespace stringify_test {
void Register(Registry& r) {  // NOLINT(misc-use-internal-linkage)
    StringifySuite::Register(r);
}
}  // namespace stringify_test
