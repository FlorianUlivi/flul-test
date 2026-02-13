#include "flul/test/runner.hpp"

#include <stdexcept>

#include "flul/test/expect.hpp"
#include "flul/test/registry.hpp"

using flul::test::Expect;
using flul::test::Registry;
using flul::test::Runner;
using flul::test::Suite;

namespace {

// NOLINTBEGIN(readability-convert-member-functions-to-static)

class PassingSuite : public Suite<PassingSuite> {
   public:
    void Pass() {}
};

class FailingSuite : public Suite<FailingSuite> {
   public:
    void FailAssert() {
        Expect(1).ToEqual(2);
    }
};

class StdExceptionSuite : public Suite<StdExceptionSuite> {
   public:
    void ThrowStd() {
        throw std::runtime_error("std error");
    }
};

class UnknownExceptionSuite : public Suite<UnknownExceptionSuite> {
   public:
    void ThrowUnknown() {
        throw 42;  // NOLINT(hicpp-exception-baseclass)
    }
};

// NOLINTEND(readability-convert-member-functions-to-static)

}  // namespace

// NOLINTBEGIN(readability-convert-member-functions-to-static,readability-make-member-function-const)

class RunnerSuite : public Suite<RunnerSuite> {
   public:
    void TestRunAllPass() {
        Registry reg;
        reg.Add<PassingSuite>("Passing", "Pass", &PassingSuite::Pass);
        Runner runner(reg);
        Expect(runner.RunAll()).ToEqual(0);
    }

    void TestRunAllFail() {
        Registry reg;
        reg.Add<FailingSuite>("Failing", "FailAssert", &FailingSuite::FailAssert);
        Runner runner(reg);
        Expect(runner.RunAll()).ToEqual(1);
    }

    void TestCatchesStdException() {
        Registry reg;
        reg.Add<StdExceptionSuite>("StdExc", "ThrowStd", &StdExceptionSuite::ThrowStd);
        Runner runner(reg);
        Expect(runner.RunAll()).ToEqual(1);
    }

    void TestCatchesUnknownException() {
        Registry reg;
        reg.Add<UnknownExceptionSuite>("Unknown", "ThrowUnknown",
                                       &UnknownExceptionSuite::ThrowUnknown);
        Runner runner(reg);
        Expect(runner.RunAll()).ToEqual(1);
    }

    static void Register(Registry& r) {
        AddTests(r, "RunnerSuite",
                 {
                     {"TestRunAllPass", &RunnerSuite::TestRunAllPass},
                     {"TestRunAllFail", &RunnerSuite::TestRunAllFail},
                     {"TestCatchesStdException", &RunnerSuite::TestCatchesStdException},
                     {"TestCatchesUnknownException", &RunnerSuite::TestCatchesUnknownException},
                 });
    }
};

// NOLINTEND(readability-convert-member-functions-to-static,readability-make-member-function-const)

namespace runner_test {
void Register(Registry& r) {  // NOLINT(misc-use-internal-linkage)
    RunnerSuite::Register(r);
}
}  // namespace runner_test
