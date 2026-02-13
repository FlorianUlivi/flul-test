#include "flul/test/expect.hpp"
#include "flul/test/registry.hpp"

using flul::test::Expect;
using flul::test::Registry;
using flul::test::Suite;

// NOLINTBEGIN(readability-convert-member-functions-to-static,readability-make-member-function-const)

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

namespace fixture_test {
void Register(Registry& r) {  // NOLINT(misc-use-internal-linkage)
    FixtureSuite::Register(r);
}
}  // namespace fixture_test
