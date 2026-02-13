#include "flul/test/registry.hpp"

#include <stdexcept>

#include "flul/test/expect.hpp"
#include "flul/test/expect_callable.hpp"

using flul::test::Expect;
using flul::test::ExpectCallable;
using flul::test::Registry;
using flul::test::Suite;

namespace {

// NOLINTBEGIN(readability-convert-member-functions-to-static)

class DummySuite : public Suite<DummySuite> {
   public:
    void Pass() {}

    void Throw() {
        throw std::runtime_error("deliberate");
    }
};

class TearDownSuite : public Suite<TearDownSuite> {
   public:
    void TearDown() override {
        g_tear_down_called = true;
    }

    void ThrowAfterSetUp() {
        throw std::runtime_error("boom");
    }

    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    static bool g_tear_down_called;
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
bool TearDownSuite::g_tear_down_called = false;

// NOLINTEND(readability-convert-member-functions-to-static)

}  // namespace

// NOLINTBEGIN(readability-convert-member-functions-to-static,readability-make-member-function-const)

class RegistrySuite : public Suite<RegistrySuite> {
   public:
    void TestAddAndTests() {
        Registry reg;
        reg.Add<DummySuite>("Dummy", "Pass", &DummySuite::Pass);
        Expect(reg.Tests().size()).ToEqual(std::size_t{1});
        Expect(reg.Tests()[0].suite_name).ToEqual(std::string_view("Dummy"));
        Expect(reg.Tests()[0].test_name).ToEqual(std::string_view("Pass"));
    }

    void TestFilter() {
        Registry reg;
        reg.Add<DummySuite>("Dummy", "Pass", &DummySuite::Pass);
        reg.Add<DummySuite>("Dummy", "Throw", &DummySuite::Throw);
        reg.Filter("Pass");
        Expect(reg.Tests().size()).ToEqual(std::size_t{1});
        Expect(reg.Tests()[0].test_name).ToEqual(std::string_view("Pass"));
    }

    void TestList() {
        Registry reg;
        reg.Add<DummySuite>("Dummy", "Pass", &DummySuite::Pass);
        reg.List();
    }

    void TestTearDownOnException() {
        Registry reg;
        reg.Add<TearDownSuite>("TearDown", "ThrowAfterSetUp", &TearDownSuite::ThrowAfterSetUp);
        TearDownSuite::g_tear_down_called = false;

        ExpectCallable([&reg] { reg.Tests()[0].callable(); }).ToThrow<std::runtime_error>();

        Expect(TearDownSuite::g_tear_down_called).ToBeTrue();
    }

    static void Register(Registry& r) {
        AddTests(r, "RegistrySuite",
                 {
                     {"TestAddAndTests", &RegistrySuite::TestAddAndTests},
                     {"TestFilter", &RegistrySuite::TestFilter},
                     {"TestList", &RegistrySuite::TestList},
                     {"TestTearDownOnException", &RegistrySuite::TestTearDownOnException},
                 });
    }
};

// NOLINTEND(readability-convert-member-functions-to-static,readability-make-member-function-const)

namespace registry_test {
void Register(Registry& r) {  // NOLINT(misc-use-internal-linkage)
    RegistrySuite::Register(r);
}
}  // namespace registry_test
