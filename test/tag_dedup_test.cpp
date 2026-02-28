#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "flul/test/expect.hpp"
#include "flul/test/registry.hpp"
#include "flul/test/run.hpp"

using flul::test::Expect;
using flul::test::Registry;
using flul::test::Suite;

namespace {

// NOLINTBEGIN(readability-convert-member-functions-to-static)

class TagDedupDummy : public Suite<TagDedupDummy> {
   public:
    void Alpha() {}
    void Beta() {}
    void Gamma() {}
};

// NOLINTEND(readability-convert-member-functions-to-static)

auto MakeArgv(std::initializer_list<const char*> args) -> std::vector<char*> {
    std::vector<char*> argv;
    for (const auto* a : args) {
        argv.push_back(const_cast<char*>(a));  // NOLINT(cppcoreguidelines-pro-type-const-cast)
    }
    return argv;
}

auto CaptureStderr(auto fn) -> std::string {
    std::ostringstream captured;  // NOLINT(misc-const-correctness)
    std::streambuf* old = std::cerr.rdbuf(captured.rdbuf());
    fn();
    std::cerr.rdbuf(old);
    return captured.str();
}

auto CountOccurrences(const std::string& haystack, const std::string& needle) -> std::size_t {
    std::size_t count = 0;
    std::size_t pos = 0;
    while ((pos = haystack.find(needle, pos)) != std::string::npos) {
        ++count;
        pos += needle.size();
    }
    return count;
}

}  // namespace

// NOLINTBEGIN(readability-convert-member-functions-to-static,readability-make-member-function-const)

class TagDedupSuite : public Suite<TagDedupSuite> {
   public:
    // --- Verify warning format matches design intent ---

    void TestWarningContainsFlulTestPrefix() {
        Registry reg;
        auto output = CaptureStderr([&] {
            reg.Add<TagDedupDummy>("MySuite", "MyTest", &TagDedupDummy::Alpha, {"dup", "dup"});
        });
        Expect(output.find("[flul-test]") != std::string::npos).ToBeTrue();
    }

    void TestWarningContainsTagName() {
        Registry reg;
        auto output = CaptureStderr([&] {
            reg.Add<TagDedupDummy>("S", "T", &TagDedupDummy::Alpha,
                                   {"specific_tag", "specific_tag"});
        });
        Expect(output.find("specific_tag") != std::string::npos).ToBeTrue();
    }

    void TestWarningContainsSuiteColonColonTestName() {
        Registry reg;
        auto output = CaptureStderr([&] {
            reg.Add<TagDedupDummy>("MySuite", "MyTest", &TagDedupDummy::Alpha, {"x", "x"});
        });
        Expect(output.find("MySuite::MyTest") != std::string::npos).ToBeTrue();
    }

    void TestWarningContainsWordWarning() {
        Registry reg;
        auto output = CaptureStderr(
            [&] { reg.Add<TagDedupDummy>("S", "T", &TagDedupDummy::Alpha, {"w", "w"}); });
        Expect(output.find("warning") != std::string::npos).ToBeTrue();
    }

    // --- Verify set ordering in --list-verbose ---

    void TestListVerboseAlphabeticalOrder() {
        Registry reg;
        reg.Add<TagDedupDummy>("S", "A", &TagDedupDummy::Alpha, {"zeta", "alpha", "mid"});
        // std::set sorts lexicographically: alpha, mid, zeta
        // Verify via iterator order (set guarantees sorted iteration)
        auto it = reg.Tests()[0].tags.begin();
        Expect(*it).ToEqual(std::string_view("alpha"));
        ++it;
        Expect(*it).ToEqual(std::string_view("mid"));
        ++it;
        Expect(*it).ToEqual(std::string_view("zeta"));
        // ListVerbose should not crash
        reg.ListVerbose();
    }

    // --- Verify dedup does not affect cross-test independence ---

    void TestDuplicateTagOnOneTestDoesNotAffectOtherTests() {
        Registry reg;
        auto output = CaptureStderr([&] {
            reg.Add<TagDedupDummy>("S", "A", &TagDedupDummy::Alpha, {"fast", "fast"});
            reg.Add<TagDedupDummy>("S", "B", &TagDedupDummy::Beta, {"fast"});
        });
        // Only one warning (from test A)
        Expect(CountOccurrences(output, "[flul-test]")).ToEqual(std::size_t{1});
        // Both tests have "fast"
        Expect(reg.Tests()[0].HasTag("fast")).ToBeTrue();
        Expect(reg.Tests()[1].HasTag("fast")).ToBeTrue();
        // A has 1 unique tag, B has 1
        Expect(reg.Tests()[0].tags.size()).ToEqual(std::size_t{1});
        Expect(reg.Tests()[1].tags.size()).ToEqual(std::size_t{1});
    }

    // --- AddTests with group-level duplicate tags ---

    void TestAddTestsWithDuplicateGroupTags() {
        Registry reg;
        auto output = CaptureStderr([&] {
            TagDedupDummy::AddTests(reg, "S",
                                    {
                                        {"Alpha", &TagDedupDummy::Alpha},
                                        {"Beta", &TagDedupDummy::Beta},
                                    },
                                    {"dup", "dup"});
        });
        // Each test should emit one warning (dup appears twice in group tags)
        Expect(CountOccurrences(output, "[flul-test]")).ToEqual(std::size_t{2});
        // Both tests have exactly 1 unique tag
        Expect(reg.Tests()[0].tags.size()).ToEqual(std::size_t{1});
        Expect(reg.Tests()[1].tags.size()).ToEqual(std::size_t{1});
        // Warning should reference both tests
        Expect(output.find("S::Alpha") != std::string::npos).ToBeTrue();
        Expect(output.find("S::Beta") != std::string::npos).ToBeTrue();
    }

    // --- Edge: all tags identical ---

    void TestAllTagsIdentical() {
        Registry reg;
        auto output = CaptureStderr([&] {
            reg.Add<TagDedupDummy>("S", "A", &TagDedupDummy::Alpha,
                                   {"same", "same", "same", "same", "same"});
        });
        // 4 duplicates (first is kept, 4 are duplicates)
        Expect(CountOccurrences(output, "[flul-test]")).ToEqual(std::size_t{4});
        Expect(reg.Tests()[0].tags.size()).ToEqual(std::size_t{1});
    }

    // --- Edge: single tag, no duplicate ---

    void TestSingleTagNoDuplicate() {
        Registry reg;
        auto output = CaptureStderr(
            [&] { reg.Add<TagDedupDummy>("S", "A", &TagDedupDummy::Alpha, {"only"}); });
        Expect(output.empty()).ToBeTrue();
        Expect(reg.Tests()[0].tags.size()).ToEqual(std::size_t{1});
    }

    // --- Edge: empty tag list ---

    void TestEmptyTagListNoWarning() {
        Registry reg;
        auto output =
            CaptureStderr([&] { reg.Add<TagDedupDummy>("S", "A", &TagDedupDummy::Alpha, {}); });
        Expect(output.empty()).ToBeTrue();
        Expect(reg.Tests()[0].tags.empty()).ToBeTrue();
    }

    // --- Edge: dedup of empty string tags ---

    void TestDuplicateEmptyStringTagDeduped() {
        Registry reg;
        auto output = CaptureStderr(
            [&] { reg.Add<TagDedupDummy>("S", "A", &TagDedupDummy::Alpha, {"", ""}); });
        Expect(CountOccurrences(output, "[flul-test]")).ToEqual(std::size_t{1});
        Expect(reg.Tests()[0].tags.size()).ToEqual(std::size_t{1});
        Expect(reg.Tests()[0].HasTag("")).ToBeTrue();
    }

    // --- FilterByTag correctness after dedup ---

    void TestFilterByTagAfterDedup() {
        Registry reg;
        CaptureStderr([&] {
            reg.Add<TagDedupDummy>("S", "A", &TagDedupDummy::Alpha,
                                   {"fast", "fast", "fast", "slow"});
        });
        // A has {fast, slow} after dedup
        std::vector<std::string_view> include = {"fast"};
        reg.FilterByTag(include);
        Expect(reg.Tests().size()).ToEqual(std::size_t{1});
    }

    void TestExcludeByTagAfterDedup() {
        Registry reg;
        CaptureStderr([&] {
            reg.Add<TagDedupDummy>("S", "A", &TagDedupDummy::Alpha, {"fast", "fast"});
            reg.Add<TagDedupDummy>("S", "B", &TagDedupDummy::Beta, {"slow"});
        });
        std::vector<std::string_view> exclude = {"fast"};
        reg.ExcludeByTag(exclude);
        Expect(reg.Tests().size()).ToEqual(std::size_t{1});
        Expect(reg.Tests()[0].test_name).ToEqual(std::string_view("B"));
    }

    // --- --list invariant: bare names even after dedup ---

    void TestListOutputBareNamesAfterDedup() {
        Registry reg;
        CaptureStderr(
            [&] { reg.Add<TagDedupDummy>("S", "A", &TagDedupDummy::Alpha, {"fast", "fast"}); });
        // Verify dedup happened correctly
        Expect(reg.Tests()[0].tags.size()).ToEqual(std::size_t{1});
        // Verify List() does not crash after dedup; output format verified via CLI
        reg.List();
        // Verify --list via Run() with deduped tags
        auto argv = MakeArgv({"prog", "--list"});
        auto rc = flul::test::Run(static_cast<int>(argv.size()), argv.data(), reg);
        Expect(rc).ToEqual(0);
    }

    // --- Case-sensitive dedup ---

    void TestCaseSensitiveDedup() {
        Registry reg;
        auto output = CaptureStderr([&] {
            reg.Add<TagDedupDummy>("S", "A", &TagDedupDummy::Alpha, {"Fast", "fast", "FAST"});
        });
        // All three are distinct; no warnings
        Expect(output.empty()).ToBeTrue();
        Expect(reg.Tests()[0].tags.size()).ToEqual(std::size_t{3});
    }

    // --- RunAll still works after dedup ---

    void TestRunAllSucceedsAfterDedup() {
        Registry reg;
        CaptureStderr([&] { reg.Add<TagDedupDummy>("S", "A", &TagDedupDummy::Alpha, {"x", "x"}); });
        auto argv = MakeArgv({"prog"});
        auto rc = flul::test::Run(static_cast<int>(argv.size()), argv.data(), reg);
        Expect(rc).ToEqual(0);
    }

    // --- Combination: dedup + filter + exclude + list ---

    void TestDedupWithFilterExcludeListCombo() {
        Registry reg;
        CaptureStderr([&] {
            reg.Add<TagDedupDummy>("S", "A", &TagDedupDummy::Alpha, {"fast", "unit", "fast"});
            reg.Add<TagDedupDummy>("S", "B", &TagDedupDummy::Beta, {"fast"});
            reg.Add<TagDedupDummy>("S", "C", &TagDedupDummy::Gamma, {"slow"});
        });
        // A: {fast, unit}, B: {fast}, C: {slow}
        auto argv = MakeArgv({"prog", "--tag", "fast", "--exclude-tag", "unit", "--list"});
        auto rc = flul::test::Run(static_cast<int>(argv.size()), argv.data(), reg);
        Expect(rc).ToEqual(0);
        // After filters: only B remains (A excluded by unit, C excluded by tag filter)
        Expect(reg.Tests().size()).ToEqual(std::size_t{1});
        Expect(reg.Tests()[0].test_name).ToEqual(std::string_view("B"));
    }

    // --- Verify set iteration in ListVerbose does not crash with deduped tags ---

    void TestListVerboseAfterHeavyDedup() {
        Registry reg;
        CaptureStderr([&] {
            reg.Add<TagDedupDummy>("S", "A", &TagDedupDummy::Alpha,
                                   {"a", "b", "c", "a", "b", "c", "a", "b", "c"});
        });
        // 3 unique tags, 6 duplicates
        Expect(reg.Tests()[0].tags.size()).ToEqual(std::size_t{3});
        // Verify sorted order via iterator
        auto it = reg.Tests()[0].tags.begin();
        Expect(*it).ToEqual(std::string_view("a"));
        ++it;
        Expect(*it).ToEqual(std::string_view("b"));
        ++it;
        Expect(*it).ToEqual(std::string_view("c"));
        // ListVerbose should not crash with deduped set
        reg.ListVerbose();
    }

    static void Register(Registry& r) {
        AddTests(
            r, "TagDedupSuite",
            {
                {"TestWarningContainsFlulTestPrefix",
                 &TagDedupSuite::TestWarningContainsFlulTestPrefix},
                {"TestWarningContainsTagName", &TagDedupSuite::TestWarningContainsTagName},
                {"TestWarningContainsSuiteColonColonTestName",
                 &TagDedupSuite::TestWarningContainsSuiteColonColonTestName},
                {"TestWarningContainsWordWarning", &TagDedupSuite::TestWarningContainsWordWarning},
                {"TestListVerboseAlphabeticalOrder",
                 &TagDedupSuite::TestListVerboseAlphabeticalOrder},
                {"TestDuplicateTagOnOneTestDoesNotAffectOtherTests",
                 &TagDedupSuite::TestDuplicateTagOnOneTestDoesNotAffectOtherTests},
                {"TestAddTestsWithDuplicateGroupTags",
                 &TagDedupSuite::TestAddTestsWithDuplicateGroupTags},
                {"TestAllTagsIdentical", &TagDedupSuite::TestAllTagsIdentical},
                {"TestSingleTagNoDuplicate", &TagDedupSuite::TestSingleTagNoDuplicate},
                {"TestEmptyTagListNoWarning", &TagDedupSuite::TestEmptyTagListNoWarning},
                {"TestDuplicateEmptyStringTagDeduped",
                 &TagDedupSuite::TestDuplicateEmptyStringTagDeduped},
                {"TestFilterByTagAfterDedup", &TagDedupSuite::TestFilterByTagAfterDedup},
                {"TestExcludeByTagAfterDedup", &TagDedupSuite::TestExcludeByTagAfterDedup},
                {"TestListOutputBareNamesAfterDedup",
                 &TagDedupSuite::TestListOutputBareNamesAfterDedup},
                {"TestCaseSensitiveDedup", &TagDedupSuite::TestCaseSensitiveDedup},
                {"TestRunAllSucceedsAfterDedup", &TagDedupSuite::TestRunAllSucceedsAfterDedup},
                {"TestDedupWithFilterExcludeListCombo",
                 &TagDedupSuite::TestDedupWithFilterExcludeListCombo},
                {"TestListVerboseAfterHeavyDedup", &TagDedupSuite::TestListVerboseAfterHeavyDedup},
            });
    }
};

// NOLINTEND(readability-convert-member-functions-to-static,readability-make-member-function-const)

namespace tag_dedup_test {
void Register(Registry& r) {  // NOLINT(misc-use-internal-linkage)
    TagDedupSuite::Register(r);
}
}  // namespace tag_dedup_test
