#include <sys/wait.h>
#include <unistd.h>

#include <cstddef>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "flul/test/expect.hpp"
#include "flul/test/registry.hpp"
#include "flul/test/run.hpp"

using flul::test::Expect;
using flul::test::Registry;
using flul::test::Suite;

namespace {

// NOLINTBEGIN(readability-convert-member-functions-to-static)

class TdAdvDummy : public Suite<TdAdvDummy> {
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

auto CaptureStdout(auto fn) -> std::string {
    // std::println writes to stdout fd directly, not through std::cout's streambuf.
    // Must redirect at the fd level.
    int pipefd[2] = {};
    pipe(pipefd);
    int saved_stdout = dup(STDOUT_FILENO);
    dup2(pipefd[1], STDOUT_FILENO);
    close(pipefd[1]);
    fn();
    fflush(stdout);  // NOLINT(misc-include-cleaner)
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);
    std::string captured;
    char buf[512] = {};
    ssize_t n = 0;
    while ((n = read(pipefd[0], buf, sizeof(buf))) > 0) {
        captured.append(buf, static_cast<std::size_t>(n));
    }
    close(pipefd[0]);
    return captured;
}

auto CaptureStderr(auto fn) -> std::string {
    std::ostringstream captured;  // NOLINT(misc-const-correctness)
    std::streambuf* old = std::cerr.rdbuf(captured.rdbuf());
    fn();
    std::cerr.rdbuf(old);
    return captured.str();
}

template <typename F>
auto DiesOnTerminate(F fn) -> bool {
    pid_t pid = fork();  // NOLINT(misc-const-correctness)
    if (pid == 0) {
        // NOLINTNEXTLINE(cppcoreguidelines-owning-memory,misc-include-cleaner)
        freopen("/dev/null", "w", stderr);
        fn();
        _Exit(0);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    if (WIFSIGNALED(status)) {  // NOLINT(hicpp-signed-bitwise)
        return true;
    }
    return WIFEXITED(status) && WEXITSTATUS(status) != 0;  // NOLINT(hicpp-signed-bitwise)
}

}  // namespace

// NOLINTBEGIN(readability-convert-member-functions-to-static,readability-make-member-function-const)

class TagTestDefAdversarialSuite : public Suite<TagTestDefAdversarialSuite> {
   public:
    // ========================================================================
    // TestDef aggregate initialization variants
    // ========================================================================

    void TestTestDefNoTagsOmitted() {
        Registry reg;
        TdAdvDummy::AddTests(reg, "S",
                             {
                                 {.name = "Alpha", .method = &TdAdvDummy::Alpha},
                             });
        Expect(reg.Tests().size()).ToEqual(std::size_t{1});
        Expect(reg.Tests()[0].metadata.tags.empty()).ToBeTrue();
    }

    void TestTestDefEmptyTagsExplicit() {
        Registry reg;
        TdAdvDummy::AddTests(reg, "S",
                             {
                                 {.name = "Alpha", .method = &TdAdvDummy::Alpha, .tags = {}},
                             });
        Expect(reg.Tests().size()).ToEqual(std::size_t{1});
        Expect(reg.Tests()[0].metadata.tags.empty()).ToBeTrue();
    }

    void TestTestDefSingleTag() {
        Registry reg;
        TdAdvDummy::AddTests(reg, "S",
                             {
                                 {.name = "Alpha", .method = &TdAdvDummy::Alpha, .tags = {"fast"}},
                             });
        Expect(reg.Tests().size()).ToEqual(std::size_t{1});
        Expect(reg.Tests()[0].metadata.HasTag("fast")).ToBeTrue();
        Expect(reg.Tests()[0].metadata.tags.size()).ToEqual(std::size_t{1});
    }

    void TestTestDefMultipleTags() {
        Registry reg;
        TdAdvDummy::AddTests(
            reg, "S",
            {
                {.name = "Alpha", .method = &TdAdvDummy::Alpha, .tags = {"fast", "unit", "core"}},
            });
        Expect(reg.Tests().size()).ToEqual(std::size_t{1});
        Expect(reg.Tests()[0].metadata.tags.size()).ToEqual(std::size_t{3});
        Expect(reg.Tests()[0].metadata.HasTag("fast")).ToBeTrue();
        Expect(reg.Tests()[0].metadata.HasTag("unit")).ToBeTrue();
        Expect(reg.Tests()[0].metadata.HasTag("core")).ToBeTrue();
    }

    // ========================================================================
    // Mixed: some TestDef with tags, some without
    // ========================================================================

    void TestMixedTagsAndNoTags() {
        Registry reg;
        TdAdvDummy::AddTests(
            reg, "S",
            {
                {.name = "Alpha", .method = &TdAdvDummy::Alpha, .tags = {"fast"}},
                {.name = "Beta", .method = &TdAdvDummy::Beta},
                {.name = "Gamma", .method = &TdAdvDummy::Gamma, .tags = {"slow", "integration"}},
                {.name = "Delta", .method = &TdAdvDummy::Delta},
            });
        Expect(reg.Tests().size()).ToEqual(std::size_t{4});
        Expect(reg.Tests()[0].metadata.HasTag("fast")).ToBeTrue();
        Expect(reg.Tests()[1].metadata.tags.empty()).ToBeTrue();
        Expect(reg.Tests()[2].metadata.HasTag("slow")).ToBeTrue();
        Expect(reg.Tests()[2].metadata.HasTag("integration")).ToBeTrue();
        Expect(reg.Tests()[3].metadata.tags.empty()).ToBeTrue();
    }

    // ========================================================================
    // Empty AddTests call (zero TestDef entries)
    // ========================================================================

    void TestEmptyAddTestsCall() {
        Registry reg;
        TdAdvDummy::AddTests(reg, "S", {});
        Expect(reg.Tests().size()).ToEqual(std::size_t{0});
    }

    // ========================================================================
    // Duplicate tags within a single TestDef
    // ========================================================================

    void TestTestDefDuplicateTagsDeduped() {
        Registry reg;
        auto output = CaptureStderr([&] {
            TdAdvDummy::AddTests(reg, "S",
                                 {
                                     {.name = "Alpha",
                                      .method = &TdAdvDummy::Alpha,
                                      .tags = {"dup", "dup", "unique"}},
                                 });
        });
        Expect(reg.Tests()[0].metadata.tags.size()).ToEqual(std::size_t{2});
        Expect(reg.Tests()[0].metadata.HasTag("dup")).ToBeTrue();
        Expect(reg.Tests()[0].metadata.HasTag("unique")).ToBeTrue();
        // Warning should be emitted
        Expect(output.find("[flul-test]") != std::string::npos).ToBeTrue();
        Expect(output.find("dup") != std::string::npos).ToBeTrue();
    }

    // ========================================================================
    // Invalid tag in a TestDef causes abort
    // ========================================================================

    void TestTestDefInvalidTagAborts() {
        Expect(DiesOnTerminate([] {
            Registry reg;
            TdAdvDummy::AddTests(
                reg, "S",
                {
                    {.name = "Alpha", .method = &TdAdvDummy::Alpha, .tags = {"inv@lid"}},
                });
        })).ToBeTrue();
    }

    void TestTestDefInvalidTagInSecondEntryAborts() {
        Expect(DiesOnTerminate([] {
            Registry reg;
            TdAdvDummy::AddTests(
                reg, "S",
                {
                    {.name = "Alpha", .method = &TdAdvDummy::Alpha, .tags = {"valid"}},
                    {.name = "Beta", .method = &TdAdvDummy::Beta, .tags = {"b@d"}},
                });
        })).ToBeTrue();
    }

    // ========================================================================
    // --list output is bare names (CTest invariant) with per-test tags
    // ========================================================================

    void TestListOutputBareNamesWithPerTestTags() {
        Registry reg;
        TdAdvDummy::AddTests(
            reg, "MySuite",
            {
                {.name = "Alpha", .method = &TdAdvDummy::Alpha, .tags = {"fast", "unit"}},
                {.name = "Beta", .method = &TdAdvDummy::Beta},
                {.name = "Gamma", .method = &TdAdvDummy::Gamma, .tags = {"slow"}},
            });
        auto output = CaptureStdout([&] { reg.List(); });
        // Must be bare names only, no brackets, no tags
        Expect(output).ToEqual(std::string("MySuite::Alpha\nMySuite::Beta\nMySuite::Gamma\n"));
    }

    // ========================================================================
    // --list-verbose output format with per-test tags
    // ========================================================================

    void TestListVerboseFormatWithPerTestTags() {
        Registry reg;
        TdAdvDummy::AddTests(
            reg, "MySuite",
            {
                {.name = "Alpha", .method = &TdAdvDummy::Alpha, .tags = {"fast", "unit"}},
                {.name = "Beta", .method = &TdAdvDummy::Beta},
                {.name = "Gamma", .method = &TdAdvDummy::Gamma, .tags = {"slow"}},
            });
        auto output = CaptureStdout([&] { reg.ListVerbose(); });
        // Tags are sorted alphabetically in the set
        // Alpha has {fast, unit}, Beta has none, Gamma has {slow}
        Expect(output).ToEqual(
            std::string("MySuite::Alpha [fast, unit]\nMySuite::Beta\nMySuite::Gamma [slow]\n"));
    }

    void TestListVerboseNoEmptyBracketsForUntagged() {
        Registry reg;
        TdAdvDummy::AddTests(reg, "S",
                             {
                                 {.name = "Alpha", .method = &TdAdvDummy::Alpha},
                             });
        auto output = CaptureStdout([&] { reg.ListVerbose(); });
        // Must NOT have "[]" or "[ ]" suffix
        Expect(output).ToEqual(std::string("S::Alpha\n"));
        Expect(output.find('[') == std::string::npos).ToBeTrue();
    }

    // ========================================================================
    // --list via Run() with per-test tags: stdout verification
    // ========================================================================

    void TestListViaRunBareNames() {
        Registry reg;
        TdAdvDummy::AddTests(reg, "S",
                             {
                                 {.name = "A", .method = &TdAdvDummy::Alpha, .tags = {"x", "y"}},
                                 {.name = "B", .method = &TdAdvDummy::Beta},
                             });
        auto argv = MakeArgv({"prog", "--list"});
        auto output = CaptureStdout([&] {
            auto rc = flul::test::Run(static_cast<int>(argv.size()), argv.data(), reg);
            Expect(rc).ToEqual(0);
        });
        Expect(output).ToEqual(std::string("S::A\nS::B\n"));
    }

    // ========================================================================
    // --list-verbose via Run() with per-test tags: stdout verification
    // ========================================================================

    void TestListVerboseViaRunFormat() {
        Registry reg;
        TdAdvDummy::AddTests(reg, "S",
                             {
                                 {.name = "A", .method = &TdAdvDummy::Alpha, .tags = {"z", "a"}},
                                 {.name = "B", .method = &TdAdvDummy::Beta},
                             });
        auto argv = MakeArgv({"prog", "--list-verbose"});
        auto output = CaptureStdout([&] {
            auto rc = flul::test::Run(static_cast<int>(argv.size()), argv.data(), reg);
            Expect(rc).ToEqual(0);
        });
        // Tags sorted: a, z
        Expect(output).ToEqual(std::string("S::A [a, z]\nS::B\n"));
    }

    // ========================================================================
    // Per-test tags interact with --tag CLI flag
    // ========================================================================

    void TestTagFlagSelectsCorrectPerTestTags() {
        Registry reg;
        TdAdvDummy::AddTests(reg, "S",
                             {
                                 {.name = "Alpha", .method = &TdAdvDummy::Alpha, .tags = {"fast"}},
                                 {.name = "Beta", .method = &TdAdvDummy::Beta, .tags = {"slow"}},
                                 {.name = "Gamma", .method = &TdAdvDummy::Gamma},
                             });
        auto argv = MakeArgv({"prog", "--tag", "fast"});
        auto rc = flul::test::Run(static_cast<int>(argv.size()), argv.data(), reg);
        Expect(rc).ToEqual(0);
        Expect(reg.Tests().size()).ToEqual(std::size_t{1});
        Expect(reg.Tests()[0].metadata.test_name).ToEqual(std::string_view("Alpha"));
    }

    // ========================================================================
    // Per-test tags interact with --exclude-tag CLI flag
    // ========================================================================

    void TestExcludeTagWithPerTestTags() {
        Registry reg;
        TdAdvDummy::AddTests(reg, "S",
                             {
                                 {.name = "Alpha", .method = &TdAdvDummy::Alpha, .tags = {"fast"}},
                                 {.name = "Beta", .method = &TdAdvDummy::Beta, .tags = {"slow"}},
                                 {.name = "Gamma", .method = &TdAdvDummy::Gamma},
                             });
        auto argv = MakeArgv({"prog", "--exclude-tag", "slow"});
        auto rc = flul::test::Run(static_cast<int>(argv.size()), argv.data(), reg);
        Expect(rc).ToEqual(0);
        Expect(reg.Tests().size()).ToEqual(std::size_t{2});
        Expect(reg.Tests()[0].metadata.test_name).ToEqual(std::string_view("Alpha"));
        Expect(reg.Tests()[1].metadata.test_name).ToEqual(std::string_view("Gamma"));
    }

    // ========================================================================
    // Per-test tags + --filter composition
    // ========================================================================

    void TestFilterAndTagComposeWithPerTestTags() {
        Registry reg;
        TdAdvDummy::AddTests(
            reg, "S",
            {
                {.name = "Alpha", .method = &TdAdvDummy::Alpha, .tags = {"fast"}},
                {.name = "AlphaSlow", .method = &TdAdvDummy::Beta, .tags = {"slow"}},
                {.name = "Beta", .method = &TdAdvDummy::Gamma, .tags = {"fast"}},
            });
        // --filter Alpha narrows to Alpha + AlphaSlow, then --tag fast keeps only Alpha
        auto argv = MakeArgv({"prog", "--filter", "Alpha", "--tag", "fast"});
        auto rc = flul::test::Run(static_cast<int>(argv.size()), argv.data(), reg);
        Expect(rc).ToEqual(0);
        Expect(reg.Tests().size()).ToEqual(std::size_t{1});
        Expect(reg.Tests()[0].metadata.test_name).ToEqual(std::string_view("Alpha"));
    }

    // ========================================================================
    // --list with --tag shows only matching tests (bare names)
    // ========================================================================

    void TestListWithTagFilterShowsBareNames() {
        Registry reg;
        TdAdvDummy::AddTests(reg, "S",
                             {
                                 {.name = "A", .method = &TdAdvDummy::Alpha, .tags = {"keep"}},
                                 {.name = "B", .method = &TdAdvDummy::Beta, .tags = {"drop"}},
                                 {.name = "C", .method = &TdAdvDummy::Gamma},
                             });
        auto argv = MakeArgv({"prog", "--list", "--tag", "keep"});
        auto output = CaptureStdout([&] {
            auto rc = flul::test::Run(static_cast<int>(argv.size()), argv.data(), reg);
            Expect(rc).ToEqual(0);
        });
        Expect(output).ToEqual(std::string("S::A\n"));
    }

    // ========================================================================
    // --list-verbose with --tag shows only matching tests (with tags)
    // ========================================================================

    void TestListVerboseWithTagFilter() {
        Registry reg;
        TdAdvDummy::AddTests(
            reg, "S",
            {
                {.name = "A", .method = &TdAdvDummy::Alpha, .tags = {"keep", "extra"}},
                {.name = "B", .method = &TdAdvDummy::Beta, .tags = {"drop"}},
            });
        auto argv = MakeArgv({"prog", "--list-verbose", "--tag", "keep"});
        auto output = CaptureStdout([&] {
            auto rc = flul::test::Run(static_cast<int>(argv.size()), argv.data(), reg);
            Expect(rc).ToEqual(0);
        });
        // Tags sorted: extra, keep
        Expect(output).ToEqual(std::string("S::A [extra, keep]\n"));
    }

    // ========================================================================
    // Old-style TestDef init (without designated initializers) still works
    // ========================================================================

    void TestOldStyleTestDefInit() {
        Registry reg;
        TdAdvDummy::AddTests(reg, "S",
                             {
                                 {"Alpha", &TdAdvDummy::Alpha},
                                 {"Beta", &TdAdvDummy::Beta, {"tag1"}},
                             });
        Expect(reg.Tests().size()).ToEqual(std::size_t{2});
        Expect(reg.Tests()[0].metadata.tags.empty()).ToBeTrue();
        Expect(reg.Tests()[1].metadata.HasTag("tag1")).ToBeTrue();
    }

    // ========================================================================
    // Large AddTests call: many TestDef entries
    // ========================================================================

    void TestLargeAddTestsCall() {
        Registry reg;
        // 4 tests, each with different tag configurations
        TdAdvDummy::AddTests(reg, "Bulk",
                             {
                                 {.name = "T1", .method = &TdAdvDummy::Alpha, .tags = {"a"}},
                                 {.name = "T2", .method = &TdAdvDummy::Beta, .tags = {"b", "c"}},
                                 {.name = "T3", .method = &TdAdvDummy::Gamma},
                                 {.name = "T4", .method = &TdAdvDummy::Delta, .tags = {"a", "d"}},
                             });
        Expect(reg.Tests().size()).ToEqual(std::size_t{4});
        // Verify tag filtering works on bulk-registered tests
        std::vector<std::string_view> include = {"a"};
        reg.FilterByTag(include);
        Expect(reg.Tests().size()).ToEqual(std::size_t{2});
        Expect(reg.Tests()[0].metadata.test_name).ToEqual(std::string_view("T1"));
        Expect(reg.Tests()[1].metadata.test_name).ToEqual(std::string_view("T4"));
    }

    // ========================================================================
    // Multiple AddTests calls accumulate correctly
    // ========================================================================

    void TestMultipleAddTestsCalls() {
        Registry reg;
        TdAdvDummy::AddTests(reg, "Suite1",
                             {
                                 {.name = "A", .method = &TdAdvDummy::Alpha, .tags = {"s1"}},
                             });
        TdAdvDummy::AddTests(reg, "Suite2",
                             {
                                 {.name = "B", .method = &TdAdvDummy::Beta, .tags = {"s2"}},
                             });
        Expect(reg.Tests().size()).ToEqual(std::size_t{2});
        Expect(reg.Tests()[0].metadata.suite_name).ToEqual(std::string_view("Suite1"));
        Expect(reg.Tests()[0].metadata.HasTag("s1")).ToBeTrue();
        Expect(reg.Tests()[1].metadata.suite_name).ToEqual(std::string_view("Suite2"));
        Expect(reg.Tests()[1].metadata.HasTag("s2")).ToBeTrue();
    }

    // ========================================================================
    // TestDef where first entry has invalid tag: second never registered
    // ========================================================================

    void TestFirstTestDefInvalidPreventsSubsequentRegistration() {
        // The abort happens on the first entry; the second is never processed
        Expect(DiesOnTerminate([] {
            Registry reg;
            TdAdvDummy::AddTests(
                reg, "S",
                {
                    {.name = "Bad", .method = &TdAdvDummy::Alpha, .tags = {"b@d"}},
                    {.name = "Good", .method = &TdAdvDummy::Beta, .tags = {"valid"}},
                });
        })).ToBeTrue();
    }

    // ========================================================================
    // --list-verbose with --exclude-tag: excluded tests are not listed
    // ========================================================================

    void TestListVerboseWithExcludeTag() {
        Registry reg;
        TdAdvDummy::AddTests(reg, "S",
                             {
                                 {.name = "A", .method = &TdAdvDummy::Alpha, .tags = {"keep"}},
                                 {.name = "B", .method = &TdAdvDummy::Beta, .tags = {"drop"}},
                             });
        auto argv = MakeArgv({"prog", "--list-verbose", "--exclude-tag", "drop"});
        auto output = CaptureStdout([&] {
            auto rc = flul::test::Run(static_cast<int>(argv.size()), argv.data(), reg);
            Expect(rc).ToEqual(0);
        });
        Expect(output).ToEqual(std::string("S::A [keep]\n"));
    }

    // ========================================================================
    // --tag + --exclude-tag + --filter + --list-verbose: full pipeline
    // ========================================================================

    void TestFullPipelineListVerbose() {
        Registry reg;
        TdAdvDummy::AddTests(
            reg, "S",
            {
                {.name = "AlphaFast", .method = &TdAdvDummy::Alpha, .tags = {"fast", "unit"}},
                {.name = "AlphaSlow", .method = &TdAdvDummy::Beta, .tags = {"slow", "unit"}},
                {.name = "BetaFast", .method = &TdAdvDummy::Gamma, .tags = {"fast"}},
                {.name = "Other", .method = &TdAdvDummy::Delta, .tags = {"fast"}},
            });
        // --filter Alpha -> AlphaFast, AlphaSlow
        // --tag fast -> AlphaFast
        // --exclude-tag (none)
        auto argv = MakeArgv({"prog", "--filter", "Alpha", "--tag", "fast", "--list-verbose"});
        auto output = CaptureStdout([&] {
            auto rc = flul::test::Run(static_cast<int>(argv.size()), argv.data(), reg);
            Expect(rc).ToEqual(0);
        });
        // Tags sorted: fast, unit
        Expect(output).ToEqual(std::string("S::AlphaFast [fast, unit]\n"));
    }

    // ========================================================================
    // --list with --tag + --exclude-tag: full pipeline bare names
    // ========================================================================

    void TestFullPipelineListBareNames() {
        Registry reg;
        TdAdvDummy::AddTests(
            reg, "S",
            {
                {.name = "A", .method = &TdAdvDummy::Alpha, .tags = {"fast", "unit"}},
                {.name = "B", .method = &TdAdvDummy::Beta, .tags = {"fast"}},
                {.name = "C", .method = &TdAdvDummy::Gamma, .tags = {"slow"}},
            });
        // --tag fast -> A, B; --exclude-tag unit -> B
        auto argv = MakeArgv({"prog", "--tag", "fast", "--exclude-tag", "unit", "--list"});
        auto output = CaptureStdout([&] {
            auto rc = flul::test::Run(static_cast<int>(argv.size()), argv.data(), reg);
            Expect(rc).ToEqual(0);
        });
        Expect(output).ToEqual(std::string("S::B\n"));
    }

    static void Register(Registry& r) {
        AddTests(
            r, "TagTestDefAdversarialSuite",
            {
                {.name = "TestTestDefNoTagsOmitted",
                 .method = &TagTestDefAdversarialSuite::TestTestDefNoTagsOmitted},
                {.name = "TestTestDefEmptyTagsExplicit",
                 .method = &TagTestDefAdversarialSuite::TestTestDefEmptyTagsExplicit},
                {.name = "TestTestDefSingleTag",
                 .method = &TagTestDefAdversarialSuite::TestTestDefSingleTag},
                {.name = "TestTestDefMultipleTags",
                 .method = &TagTestDefAdversarialSuite::TestTestDefMultipleTags},
                {.name = "TestMixedTagsAndNoTags",
                 .method = &TagTestDefAdversarialSuite::TestMixedTagsAndNoTags},
                {.name = "TestEmptyAddTestsCall",
                 .method = &TagTestDefAdversarialSuite::TestEmptyAddTestsCall},
                {.name = "TestTestDefDuplicateTagsDeduped",
                 .method = &TagTestDefAdversarialSuite::TestTestDefDuplicateTagsDeduped},
                {.name = "TestTestDefInvalidTagAborts",
                 .method = &TagTestDefAdversarialSuite::TestTestDefInvalidTagAborts},
                {.name = "TestTestDefInvalidTagInSecondEntryAborts",
                 .method = &TagTestDefAdversarialSuite::TestTestDefInvalidTagInSecondEntryAborts},
                {.name = "TestListOutputBareNamesWithPerTestTags",
                 .method = &TagTestDefAdversarialSuite::TestListOutputBareNamesWithPerTestTags},
                {.name = "TestListVerboseFormatWithPerTestTags",
                 .method = &TagTestDefAdversarialSuite::TestListVerboseFormatWithPerTestTags},
                {.name = "TestListVerboseNoEmptyBracketsForUntagged",
                 .method = &TagTestDefAdversarialSuite::TestListVerboseNoEmptyBracketsForUntagged},
                {.name = "TestListViaRunBareNames",
                 .method = &TagTestDefAdversarialSuite::TestListViaRunBareNames},
                {.name = "TestListVerboseViaRunFormat",
                 .method = &TagTestDefAdversarialSuite::TestListVerboseViaRunFormat},
                {.name = "TestTagFlagSelectsCorrectPerTestTags",
                 .method = &TagTestDefAdversarialSuite::TestTagFlagSelectsCorrectPerTestTags},
                {.name = "TestExcludeTagWithPerTestTags",
                 .method = &TagTestDefAdversarialSuite::TestExcludeTagWithPerTestTags},
                {.name = "TestFilterAndTagComposeWithPerTestTags",
                 .method = &TagTestDefAdversarialSuite::TestFilterAndTagComposeWithPerTestTags},
                {.name = "TestListWithTagFilterShowsBareNames",
                 .method = &TagTestDefAdversarialSuite::TestListWithTagFilterShowsBareNames},
                {.name = "TestListVerboseWithTagFilter",
                 .method = &TagTestDefAdversarialSuite::TestListVerboseWithTagFilter},
                {.name = "TestOldStyleTestDefInit",
                 .method = &TagTestDefAdversarialSuite::TestOldStyleTestDefInit},
                {.name = "TestLargeAddTestsCall",
                 .method = &TagTestDefAdversarialSuite::TestLargeAddTestsCall},
                {.name = "TestMultipleAddTestsCalls",
                 .method = &TagTestDefAdversarialSuite::TestMultipleAddTestsCalls},
                {.name = "TestFirstTestDefInvalidPreventsSubsequentRegistration",
                 .method = &TagTestDefAdversarialSuite::
                               TestFirstTestDefInvalidPreventsSubsequentRegistration},
                {.name = "TestListVerboseWithExcludeTag",
                 .method = &TagTestDefAdversarialSuite::TestListVerboseWithExcludeTag},
                {.name = "TestFullPipelineListVerbose",
                 .method = &TagTestDefAdversarialSuite::TestFullPipelineListVerbose},
                {.name = "TestFullPipelineListBareNames",
                 .method = &TagTestDefAdversarialSuite::TestFullPipelineListBareNames},
            });
    }
};

// NOLINTEND(readability-convert-member-functions-to-static,readability-make-member-function-const)

namespace tag_testdef_adversarial_test {
void Register(Registry& r) {  // NOLINT(misc-use-internal-linkage)
    TagTestDefAdversarialSuite::Register(r);
}
}  // namespace tag_testdef_adversarial_test
