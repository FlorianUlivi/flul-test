#include "flul/test/run.hpp"

#include "flul/test/expect.hpp"
#include "flul/test/registry.hpp"

using flul::test::Expect;
using flul::test::Registry;
using flul::test::Suite;

namespace {

class DummySuite : public Suite<DummySuite> {
   public:
    void Pass() {}
};

auto MakeArgv(std::initializer_list<const char*> args) -> std::vector<char*> {
    std::vector<char*> argv;
    for (const auto* a : args) {
        argv.push_back(const_cast<char*>(a));  // NOLINT(cppcoreguidelines-pro-type-const-cast)
    }
    return argv;
}

}  // namespace

// NOLINTBEGIN(readability-convert-member-functions-to-static,readability-make-member-function-const)

class RunSuite : public Suite<RunSuite> {
   public:
    void TestList() {
        Registry reg;
        reg.Add<DummySuite>("Dummy", "Pass", &DummySuite::Pass);
        auto argv = MakeArgv({"prog", "--list"});
        Expect(flul::test::Run(static_cast<int>(argv.size()), argv.data(), reg)).ToEqual(0);
    }

    void TestFilterWorks() {
        Registry reg;
        reg.Add<DummySuite>("Dummy", "Pass", &DummySuite::Pass);
        auto argv = MakeArgv({"prog", "--filter", "Dummy"});
        Expect(flul::test::Run(static_cast<int>(argv.size()), argv.data(), reg)).ToEqual(0);
    }

    void TestFilterMissingArg() {
        Registry reg;
        auto argv = MakeArgv({"prog", "--filter"});
        Expect(flul::test::Run(static_cast<int>(argv.size()), argv.data(), reg)).ToEqual(1);
    }

    void TestHelp() {
        Registry reg;
        auto argv = MakeArgv({"prog", "--help"});
        Expect(flul::test::Run(static_cast<int>(argv.size()), argv.data(), reg)).ToEqual(0);
    }

    void TestUnknownOption() {
        Registry reg;
        auto argv = MakeArgv({"prog", "--bogus"});
        Expect(flul::test::Run(static_cast<int>(argv.size()), argv.data(), reg)).ToEqual(1);
    }

    static void Register(Registry& r) {
        AddTests(r, "RunSuite",
                 {
                     {"TestList", &RunSuite::TestList},
                     {"TestFilterWorks", &RunSuite::TestFilterWorks},
                     {"TestFilterMissingArg", &RunSuite::TestFilterMissingArg},
                     {"TestHelp", &RunSuite::TestHelp},
                     {"TestUnknownOption", &RunSuite::TestUnknownOption},
                 });
    }
};

// NOLINTEND(readability-convert-member-functions-to-static,readability-make-member-function-const)

namespace run_test {
void Register(Registry& r) {  // NOLINT(misc-use-internal-linkage)
    RunSuite::Register(r);
}
}  // namespace run_test
