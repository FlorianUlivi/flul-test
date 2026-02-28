#include <sys/wait.h>
#include <unistd.h>

#include <iostream>
#include <sstream>

#include "flul/test/expect.hpp"
#include "flul/test/registry.hpp"
#include "flul/test/run.hpp"

using flul::test::Expect;
using flul::test::Registry;
using flul::test::Suite;

namespace {

// NOLINTBEGIN(readability-convert-member-functions-to-static)

class TagAdvDummy : public Suite<TagAdvDummy> {
   public:
    void Alpha() {}
    void Beta() {}
    void Gamma() {}
    void Delta() {}
};

// NOLINTEND(readability-convert-member-functions-to-static)

auto MakeArgv(std::initializer_list<const char*> args) -> std::vector<char*> {
    std::vector<char*> argv;
    for (const auto* a : args) {
        argv.push_back(const_cast<char*>(a));  // NOLINT(cppcoreguidelines-pro-type-const-cast)
    }
    return argv;
}

// Runs `fn` in a forked child process. Returns true if the child terminated
// abnormally (signal or non-zero exit), which is the expected outcome when
// std::terminate() is called.
template <typename F>
auto DiesOnTerminate(F fn) -> bool {
    pid_t pid = fork();  // NOLINT(misc-const-correctness)
    if (pid == 0) {
        // Child: redirect stderr to /dev/null to suppress diagnostic output
        // NOLINTNEXTLINE(cppcoreguidelines-owning-memory,misc-include-cleaner)
        freopen("/dev/null", "w", stderr);
        fn();
        // If fn() returns without terminating, exit normally (test should fail)
        _Exit(0);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    // Terminated by signal (e.g. SIGABRT from std::terminate/std::abort)
    if (WIFSIGNALED(status)) {  // NOLINT(hicpp-signed-bitwise)
        return true;
    }
    // Or exited with non-zero status (e.g. some platforms call _exit(1))
    return WIFEXITED(status) && WEXITSTATUS(status) != 0;  // NOLINT(hicpp-signed-bitwise)
}

}  // namespace

// NOLINTBEGIN(readability-convert-member-functions-to-static,readability-make-member-function-const)

class TagAdversarialSuite : public Suite<TagAdversarialSuite> {
   public:
    // --- Boundary: empty tag string — now causes std::terminate() ---

    void TestEmptyStringTag() {
        Expect(DiesOnTerminate([] {
            Registry reg;
            reg.Add<TagAdvDummy>("S", "A", &TagAdvDummy::Alpha, {""});
        })).ToBeTrue();
    }

    void TestFilterByEmptyStringTag() {
        // Empty tag is now invalid: Add aborts before any filtering occurs.
        Expect(DiesOnTerminate([] {
            Registry reg;
            reg.Add<TagAdvDummy>("S", "A", &TagAdvDummy::Alpha, {""});
        })).ToBeTrue();
    }

    // --- Boundary: duplicate tags on same test ---

    void TestDuplicateTagsOnSameTest() {
        Registry reg;
        reg.Add<TagAdvDummy>("S", "A", &TagAdvDummy::Alpha, {"fast", "fast", "fast"});
        // Duplicates are deduplicated at registration time; only one "fast" stored
        Expect(reg.Tests()[0].metadata.tags.size()).ToEqual(std::size_t{1});
        Expect(reg.Tests()[0].metadata.HasTag("fast")).ToBeTrue();
    }

    void TestDuplicateTagEmitsStderrWarning() {
        Registry reg;
        // Redirect stderr to capture warnings
        std::ostringstream captured;  // NOLINT(misc-const-correctness)
        std::streambuf* old_stderr = std::cerr.rdbuf(captured.rdbuf());
        reg.Add<TagAdvDummy>("S", "A", &TagAdvDummy::Alpha, {"fast", "slow", "fast"});
        std::cerr.rdbuf(old_stderr);
        // One duplicate ("fast" appears twice: second occurrence triggers warning)
        auto output = captured.str();
        Expect(output.empty()).ToBeFalse();
        Expect(output.contains("fast")).ToBeTrue();
        Expect(output.contains("S::A")).ToBeTrue();
        // Only two unique tags stored
        Expect(reg.Tests()[0].metadata.tags.size()).ToEqual(std::size_t{2});
    }

    void TestNoDuplicateTagNoWarning() {
        Registry reg;
        std::ostringstream captured;  // NOLINT(misc-const-correctness)
        std::streambuf* old_stderr = std::cerr.rdbuf(captured.rdbuf());
        reg.Add<TagAdvDummy>("S", "A", &TagAdvDummy::Alpha, {"fast", "slow"});
        std::cerr.rdbuf(old_stderr);
        // No duplicates: no warning emitted
        Expect(captured.str().empty()).ToBeTrue();
        Expect(reg.Tests()[0].metadata.tags.size()).ToEqual(std::size_t{2});
    }

    void TestMultipleDuplicatesEmitOneWarningEach() {
        Registry reg;
        std::ostringstream captured;  // NOLINT(misc-const-correctness)
        std::streambuf* old_stderr = std::cerr.rdbuf(captured.rdbuf());
        // "fast" appears 3 times total: 2 duplicates; "slow" appears 2 times: 1 duplicate
        reg.Add<TagAdvDummy>("S", "B", &TagAdvDummy::Beta,
                             {"fast", "slow", "fast", "fast", "slow"});
        std::cerr.rdbuf(old_stderr);
        auto output = captured.str();
        // 3 warnings total (2 for "fast", 1 for "slow")
        std::size_t warn_count = 0;
        std::size_t pos = 0;
        while ((pos = output.find("[flul-test]", pos)) != std::string::npos) {
            ++warn_count;
            ++pos;
        }
        Expect(warn_count).ToEqual(std::size_t{3});
        // Only 2 unique tags stored
        Expect(reg.Tests()[0].metadata.tags.size()).ToEqual(std::size_t{2});
    }

    void TestFilterByTagWithDuplicatesInInclude() {
        Registry reg;
        reg.Add<TagAdvDummy>("S", "A", &TagAdvDummy::Alpha, {"fast"});
        reg.Add<TagAdvDummy>("S", "B", &TagAdvDummy::Beta, {"slow"});
        std::vector<std::string_view> include = {"fast", "fast", "fast"};
        reg.FilterByTag(include);
        Expect(reg.Tests().size()).ToEqual(std::size_t{1});
        Expect(reg.Tests()[0].metadata.test_name).ToEqual(std::string_view("A"));
    }

    // --- Boundary: case sensitivity ---

    void TestTagIsCaseSensitive() {
        Registry reg;
        reg.Add<TagAdvDummy>("S", "A", &TagAdvDummy::Alpha, {"Fast"});
        Expect(reg.Tests()[0].metadata.HasTag("Fast")).ToBeTrue();
        Expect(reg.Tests()[0].metadata.HasTag("fast")).ToBeFalse();
        Expect(reg.Tests()[0].metadata.HasTag("FAST")).ToBeFalse();
    }

    // --- Boundary: single test, single tag ---

    void TestSingleTestSingleTag() {
        Registry reg;
        reg.Add<TagAdvDummy>("S", "A", &TagAdvDummy::Alpha, {"only"});
        std::vector<std::string_view> include = {"only"};
        reg.FilterByTag(include);
        Expect(reg.Tests().size()).ToEqual(std::size_t{1});
    }

    // --- Boundary: all tests excluded ---

    void TestAllTestsExcluded() {
        Registry reg;
        reg.Add<TagAdvDummy>("S", "A", &TagAdvDummy::Alpha, {"x"});
        reg.Add<TagAdvDummy>("S", "B", &TagAdvDummy::Beta, {"x"});
        std::vector<std::string_view> exclude = {"x"};
        reg.ExcludeByTag(exclude);
        Expect(reg.Tests().size()).ToEqual(std::size_t{0});
    }

    void TestFilterByTagRemovesAllWhenNoneMatch() {
        Registry reg;
        reg.Add<TagAdvDummy>("S", "A", &TagAdvDummy::Alpha, {"a"});
        reg.Add<TagAdvDummy>("S", "B", &TagAdvDummy::Beta, {"b"});
        std::vector<std::string_view> include = {"nonexistent"};
        reg.FilterByTag(include);
        Expect(reg.Tests().size()).ToEqual(std::size_t{0});
    }

    // --- Boundary: no tests registered ---

    void TestFilterByTagOnEmptyRegistry() {
        Registry reg;
        std::vector<std::string_view> include = {"fast"};
        reg.FilterByTag(include);
        Expect(reg.Tests().size()).ToEqual(std::size_t{0});
    }

    void TestExcludeByTagOnEmptyRegistry() {
        Registry reg;
        std::vector<std::string_view> exclude = {"fast"};
        reg.ExcludeByTag(exclude);
        Expect(reg.Tests().size()).ToEqual(std::size_t{0});
    }

    void TestListVerboseOnEmptyRegistry() {
        Registry reg;
        reg.ListVerbose();  // should not crash
    }

    void TestListOnEmptyRegistry() {
        Registry reg;
        reg.List();  // should not crash
    }

    // --- Combination: filter + tag + exclude-tag all together ---

    void TestFilterThenTagThenExcludeCompose() {
        Registry reg;
        reg.Add<TagAdvDummy>("S", "Alpha", &TagAdvDummy::Alpha, {"fast", "unit"});
        reg.Add<TagAdvDummy>("S", "Beta", &TagAdvDummy::Beta, {"fast"});
        reg.Add<TagAdvDummy>("S", "Gamma", &TagAdvDummy::Gamma, {"slow"});
        reg.Add<TagAdvDummy>("S", "Delta", &TagAdvDummy::Delta, {"fast", "unit"});

        // Filter by name: keep only those containing "a" (Alpha, Beta, Gamma, Delta all match)
        reg.Filter("a");
        // At this point: Alpha, Beta, Gamma, Delta (all contain lowercase 'a' in suite::test)
        // Actually "S::Alpha" contains 'a', "S::Beta" contains 'a', "S::Gamma" contains 'a',
        // "S::Delta" contains 'a' => all survive

        std::vector<std::string_view> include = {"fast"};
        reg.FilterByTag(include);
        // Now: Alpha(fast,unit), Beta(fast), Delta(fast,unit)

        std::vector<std::string_view> exclude = {"unit"};
        reg.ExcludeByTag(exclude);
        // Now: Beta(fast) only

        Expect(reg.Tests().size()).ToEqual(std::size_t{1});
        Expect(reg.Tests()[0].metadata.test_name).ToEqual(std::string_view("Beta"));
    }

    // --- Invariant: --list must remain CTest-safe (bare names only) ---

    void TestListOutputNeverIncludesTags() {
        Registry reg;
        reg.Add<TagAdvDummy>("S", "A", &TagAdvDummy::Alpha, {"fast", "math"});
        reg.Add<TagAdvDummy>("S", "B", &TagAdvDummy::Beta);
        // Run with --list and --tag; output should be bare names
        auto argv = MakeArgv({"prog", "--list", "--tag", "fast"});
        auto rc = flul::test::Run(static_cast<int>(argv.size()), argv.data(), reg);
        Expect(rc).ToEqual(0);
        // After Run with --tag fast, only A survives
        Expect(reg.Tests().size()).ToEqual(std::size_t{1});
    }

    // --- Ordering: double-call FilterByTag ---

    void TestDoubleFilterByTag() {
        Registry reg;
        reg.Add<TagAdvDummy>("S", "A", &TagAdvDummy::Alpha, {"fast", "math"});
        reg.Add<TagAdvDummy>("S", "B", &TagAdvDummy::Beta, {"slow"});
        reg.Add<TagAdvDummy>("S", "C", &TagAdvDummy::Gamma, {"fast"});

        std::vector<std::string_view> include1 = {"fast"};
        reg.FilterByTag(include1);
        // A and C remain
        Expect(reg.Tests().size()).ToEqual(std::size_t{2});

        // Second FilterByTag should further narrow
        std::vector<std::string_view> include2 = {"math"};
        reg.FilterByTag(include2);
        // Only A has "math"
        Expect(reg.Tests().size()).ToEqual(std::size_t{1});
        Expect(reg.Tests()[0].metadata.test_name).ToEqual(std::string_view("A"));
    }

    // --- Ordering: ExcludeByTag before FilterByTag ---

    void TestExcludeBeforeFilterReverseOrder() {
        Registry reg;
        reg.Add<TagAdvDummy>("S", "A", &TagAdvDummy::Alpha, {"fast", "slow"});
        reg.Add<TagAdvDummy>("S", "B", &TagAdvDummy::Beta, {"fast"});
        reg.Add<TagAdvDummy>("S", "C", &TagAdvDummy::Gamma, {"slow"});

        // Apply in reverse order: exclude first, then filter
        std::vector<std::string_view> exclude = {"slow"};
        reg.ExcludeByTag(exclude);
        // B remains (A and C had "slow")
        Expect(reg.Tests().size()).ToEqual(std::size_t{1});

        std::vector<std::string_view> include = {"fast"};
        reg.FilterByTag(include);
        // B has "fast", so it remains
        Expect(reg.Tests().size()).ToEqual(std::size_t{1});
        Expect(reg.Tests()[0].metadata.test_name).ToEqual(std::string_view("B"));
    }

    // --- Large number of tags ---

    void TestManyTagsOnOneTest() {
        Registry reg;
        // Register with initializer_list of 20 tags
        reg.Add<TagAdvDummy>(
            "S", "A", &TagAdvDummy::Alpha,
            {"t0",  "t1",  "t2",  "t3",  "t4",  "t5",  "t6",  "t7",  "t8",  "t9",
             "t10", "t11", "t12", "t13", "t14", "t15", "t16", "t17", "t18", "t19"});
        Expect(reg.Tests()[0].metadata.tags.size()).ToEqual(std::size_t{20});
        Expect(reg.Tests()[0].metadata.HasTag("t0")).ToBeTrue();
        Expect(reg.Tests()[0].metadata.HasTag("t19")).ToBeTrue();
        Expect(reg.Tests()[0].metadata.HasTag("t20")).ToBeFalse();
    }

    // --- Edge: tag that looks like a CLI flag (hyphens are valid) ---

    void TestTagValueLooksLikeFlag() {
        Registry reg;
        reg.Add<TagAdvDummy>("S", "A", &TagAdvDummy::Alpha, {"--fast"});
        Expect(reg.Tests()[0].metadata.HasTag("--fast")).ToBeTrue();
        // Can we filter by it via CLI?
        auto argv = MakeArgv({"prog", "--tag", "--fast", "--list"});
        // This is ambiguous: "--fast" is parsed as the value of --tag
        // The implementation should treat it as a tag value
        auto rc = flul::test::Run(static_cast<int>(argv.size()), argv.data(), reg);
        Expect(rc).ToEqual(0);
    }

    // --- Edge: --list-verbose with single tag ---

    void TestListVerboseSingleTag() {
        Registry reg;
        reg.Add<TagAdvDummy>("S", "A", &TagAdvDummy::Alpha, {"fast"});
        reg.ListVerbose();  // Should output "S::A [fast]"
    }

    // --- Edge: --list and --list-verbose both specified ---

    void TestListAndListVerboseBothSpecified() {
        Registry reg;
        reg.Add<TagAdvDummy>("S", "A", &TagAdvDummy::Alpha, {"fast"});
        // Both --list and --list-verbose: --list takes precedence (checked first)
        auto argv = MakeArgv({"prog", "--list", "--list-verbose"});
        auto rc = flul::test::Run(static_cast<int>(argv.size()), argv.data(), reg);
        Expect(rc).ToEqual(0);
    }

    // --- Edge: --list-verbose then --list (reversed order) ---

    void TestListVerboseThenListReversedCliOrder() {
        Registry reg;
        reg.Add<TagAdvDummy>("S", "A", &TagAdvDummy::Alpha, {"fast"});
        auto argv = MakeArgv({"prog", "--list-verbose", "--list"});
        auto rc = flul::test::Run(static_cast<int>(argv.size()), argv.data(), reg);
        Expect(rc).ToEqual(0);
        // Both flags are set; --list is checked first in code regardless of CLI order
    }

    // --- Edge: Run with --tag that matches nothing (exit code 0 with 0 tests) ---

    void TestRunWithTagMatchingNothingExitsZero() {
        Registry reg;
        reg.Add<TagAdvDummy>("S", "A", &TagAdvDummy::Alpha, {"fast"});
        auto argv = MakeArgv({"prog", "--tag", "nonexistent"});
        auto rc = flul::test::Run(static_cast<int>(argv.size()), argv.data(), reg);
        // 0 tests run = all_of(empty, passed) = true => exit 0
        Expect(rc).ToEqual(0);
    }

    // --- Edge: Run with 0 tests (all excluded) exits 0 ---

    void TestRunWithAllTestsExcludedExitsZero() {
        Registry reg;
        reg.Add<TagAdvDummy>("S", "A", &TagAdvDummy::Alpha, {"x"});
        auto argv = MakeArgv({"prog", "--exclude-tag", "x"});
        auto rc = flul::test::Run(static_cast<int>(argv.size()), argv.data(), reg);
        Expect(rc).ToEqual(0);
    }

    // --- Edge: tags containing special characters — now causes std::terminate() ---

    void TestTagWithSpecialCharacters() {
        // "tag with spaces" contains spaces which are not in [a-zA-Z0-9_-]
        Expect(DiesOnTerminate([] {
            Registry reg;
            reg.Add<TagAdvDummy>("S", "A", &TagAdvDummy::Alpha, {"tag-with-hyphens", "tag/slash"});
        })).ToBeTrue();
    }

    // --- Edge: tag containing bracket characters — now causes std::terminate() ---

    void TestTagContainingBrackets() {
        Expect(DiesOnTerminate([] {
            Registry reg;
            reg.Add<TagAdvDummy>("S", "A", &TagAdvDummy::Alpha, {"[tricky]", "normal"});
        })).ToBeTrue();
    }

    // --- Multiple exclude tags ---

    void TestMultipleExcludeTagFlags() {
        Registry reg;
        reg.Add<TagAdvDummy>("S", "A", &TagAdvDummy::Alpha, {"slow"});
        reg.Add<TagAdvDummy>("S", "B", &TagAdvDummy::Beta, {"flaky"});
        reg.Add<TagAdvDummy>("S", "C", &TagAdvDummy::Gamma, {"fast"});
        auto argv = MakeArgv({"prog", "--exclude-tag", "slow", "--exclude-tag", "flaky"});
        auto rc = flul::test::Run(static_cast<int>(argv.size()), argv.data(), reg);
        Expect(rc).ToEqual(0);
        Expect(reg.Tests().size()).ToEqual(std::size_t{1});
        Expect(reg.Tests()[0].metadata.test_name).ToEqual(std::string_view("C"));
    }

    // --- Edge: tag with only whitespace — now causes std::terminate() ---

    void TestWhitespaceOnlyTag() {
        Expect(DiesOnTerminate([] {
            Registry reg;
            reg.Add<TagAdvDummy>("S", "A", &TagAdvDummy::Alpha, {" "});
        })).ToBeTrue();
    }

    // --- Validation: diagnostic message mentions the offending tag ---

    void TestInvalidTagDiagnosticMentionsTag() {
        // Run in a child so the parent can capture stderr output before terminate
        // We use a pipe to transfer the stderr content from child to parent.
        int pipefd[2] = {};
        pipe(pipefd);

        pid_t pid = fork();  // NOLINT(misc-const-correctness)
        if (pid == 0) {
            close(pipefd[0]);
            dup2(pipefd[1], STDERR_FILENO);
            close(pipefd[1]);
            Registry reg;
            reg.Add<TagAdvDummy>("S", "A", &TagAdvDummy::Alpha, {"bad tag"});
            _Exit(0);
        }

        close(pipefd[1]);
        std::string captured;
        char buf[256] = {};
        ssize_t n = 0;
        while ((n = read(pipefd[0], buf, sizeof(buf))) > 0) {
            captured.append(buf, static_cast<std::size_t>(n));
        }
        close(pipefd[0]);
        int status = 0;
        waitpid(pid, &status, 0);

        // Child must have terminated abnormally
        // NOLINTBEGIN(misc-const-correctness)
        bool died =
            WIFSIGNALED(status) ||                            // NOLINT(hicpp-signed-bitwise)
            (WIFEXITED(status) && WEXITSTATUS(status) != 0);  // NOLINT(hicpp-signed-bitwise)
        // NOLINTEND(misc-const-correctness)
        Expect(died).ToBeTrue();
        // Diagnostic must mention the bad tag
        Expect(captured.contains("bad tag")).ToBeTrue();
    }

    static void Register(Registry& r) {
        AddTests(
            r, "TagAdversarialSuite",
            {
                {"TestEmptyStringTag", &TagAdversarialSuite::TestEmptyStringTag},
                {"TestFilterByEmptyStringTag", &TagAdversarialSuite::TestFilterByEmptyStringTag},
                {"TestDuplicateTagsOnSameTest", &TagAdversarialSuite::TestDuplicateTagsOnSameTest},
                {"TestDuplicateTagEmitsStderrWarning",
                 &TagAdversarialSuite::TestDuplicateTagEmitsStderrWarning},
                {"TestNoDuplicateTagNoWarning", &TagAdversarialSuite::TestNoDuplicateTagNoWarning},
                {"TestMultipleDuplicatesEmitOneWarningEach",
                 &TagAdversarialSuite::TestMultipleDuplicatesEmitOneWarningEach},
                {"TestFilterByTagWithDuplicatesInInclude",
                 &TagAdversarialSuite::TestFilterByTagWithDuplicatesInInclude},
                {"TestTagIsCaseSensitive", &TagAdversarialSuite::TestTagIsCaseSensitive},
                {"TestSingleTestSingleTag", &TagAdversarialSuite::TestSingleTestSingleTag},
                {"TestAllTestsExcluded", &TagAdversarialSuite::TestAllTestsExcluded},
                {"TestFilterByTagRemovesAllWhenNoneMatch",
                 &TagAdversarialSuite::TestFilterByTagRemovesAllWhenNoneMatch},
                {"TestFilterByTagOnEmptyRegistry",
                 &TagAdversarialSuite::TestFilterByTagOnEmptyRegistry},
                {"TestExcludeByTagOnEmptyRegistry",
                 &TagAdversarialSuite::TestExcludeByTagOnEmptyRegistry},
                {"TestListVerboseOnEmptyRegistry",
                 &TagAdversarialSuite::TestListVerboseOnEmptyRegistry},
                {"TestListOnEmptyRegistry", &TagAdversarialSuite::TestListOnEmptyRegistry},
                {"TestFilterThenTagThenExcludeCompose",
                 &TagAdversarialSuite::TestFilterThenTagThenExcludeCompose},
                {"TestDoubleFilterByTag", &TagAdversarialSuite::TestDoubleFilterByTag},
                {"TestExcludeBeforeFilterReverseOrder",
                 &TagAdversarialSuite::TestExcludeBeforeFilterReverseOrder},
                {"TestManyTagsOnOneTest", &TagAdversarialSuite::TestManyTagsOnOneTest},
                {"TestTagValueLooksLikeFlag", &TagAdversarialSuite::TestTagValueLooksLikeFlag},
                {"TestListVerboseSingleTag", &TagAdversarialSuite::TestListVerboseSingleTag},
                {"TestListAndListVerboseBothSpecified",
                 &TagAdversarialSuite::TestListAndListVerboseBothSpecified},
                {"TestListVerboseThenListReversedCliOrder",
                 &TagAdversarialSuite::TestListVerboseThenListReversedCliOrder},
                {"TestRunWithTagMatchingNothingExitsZero",
                 &TagAdversarialSuite::TestRunWithTagMatchingNothingExitsZero},
                {"TestRunWithAllTestsExcludedExitsZero",
                 &TagAdversarialSuite::TestRunWithAllTestsExcludedExitsZero},
                {"TestTagWithSpecialCharacters",
                 &TagAdversarialSuite::TestTagWithSpecialCharacters},
                {"TestTagContainingBrackets", &TagAdversarialSuite::TestTagContainingBrackets},
                {"TestMultipleExcludeTagFlags", &TagAdversarialSuite::TestMultipleExcludeTagFlags},
                {"TestWhitespaceOnlyTag", &TagAdversarialSuite::TestWhitespaceOnlyTag},
                {"TestInvalidTagDiagnosticMentionsTag",
                 &TagAdversarialSuite::TestInvalidTagDiagnosticMentionsTag},
            });
    }
};

// NOLINTEND(readability-convert-member-functions-to-static,readability-make-member-function-const)

namespace tag_adversarial_test {
void Register(Registry& r) {  // NOLINT(misc-use-internal-linkage)
    TagAdversarialSuite::Register(r);
}
}  // namespace tag_adversarial_test
