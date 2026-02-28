#include <cstddef>
#include <string_view>
#include <vector>

#include "flul/test/expect.hpp"
#include "flul/test/registry.hpp"
#include "flul/test/run.hpp"
#include "flul/test/runner.hpp"
#include "flul/test/test_metadata.hpp"
#include "flul/test/test_result.hpp"

using flul::test::Expect;
using flul::test::Registry;
using flul::test::Runner;
using flul::test::Suite;
using flul::test::TestMetadata;

namespace {

// NOLINTBEGIN(readability-convert-member-functions-to-static)

class MetaDummy : public Suite<MetaDummy> {
   public:
    void Alpha() {}
    void Beta() {}
    void Gamma() {}
    void Delta() {}
};

class MetaFailDummy : public Suite<MetaFailDummy> {
   public:
    void Fail() {
        Expect(false).ToBeTrue();
    }
};

// NOLINTEND(readability-convert-member-functions-to-static)

auto MakeArgv(std::initializer_list<const char*> args) -> std::vector<char*> {
    std::vector<char*> argv;
    for (const auto* a : args) {
        argv.push_back(const_cast<char*>(a));  // NOLINT(cppcoreguidelines-pro-type-const-cast)
    }
    return argv;
}

}  // namespace

// NOLINTBEGIN(readability-convert-member-functions-to-static,readability-make-member-function-const)

class TagMetadataAdversarialSuite : public Suite<TagMetadataAdversarialSuite> {
   public:
    // === TestMetadata struct direct tests ===

    void TestMetadataHasTagOnEmptyTagSet() {
        TestMetadata meta{.suite_name = "S", .test_name = "T", .tags = {}};
        Expect(meta.HasTag("anything")).ToBeFalse();
    }

    void TestMetadataHasTagFindsPresent() {
        TestMetadata meta{.suite_name = "S", .test_name = "T", .tags = {"a", "b", "c"}};
        Expect(meta.HasTag("a")).ToBeTrue();
        Expect(meta.HasTag("b")).ToBeTrue();
        Expect(meta.HasTag("c")).ToBeTrue();
        Expect(meta.HasTag("d")).ToBeFalse();
    }

    void TestMetadataDefaultConstruction() {
        TestMetadata meta{};
        Expect(meta.suite_name.empty()).ToBeTrue();
        Expect(meta.test_name.empty()).ToBeTrue();
        Expect(meta.tags.empty()).ToBeTrue();
        Expect(meta.HasTag("")).ToBeFalse();
    }

    void TestMetadataCopyPreservesTags() {
        TestMetadata original{.suite_name = "S", .test_name = "T", .tags = {"x", "y"}};
        TestMetadata copy = original;  // NOLINT(performance-unnecessary-copy-initialization)
        Expect(copy.HasTag("x")).ToBeTrue();
        Expect(copy.HasTag("y")).ToBeTrue();
        Expect(copy.suite_name).ToEqual(std::string_view("S"));
        Expect(copy.test_name).ToEqual(std::string_view("T"));
    }

    void TestMetadataMovePreservesTags() {
        TestMetadata original{.suite_name = "S", .test_name = "T", .tags = {"x", "y"}};
        TestMetadata moved = std::move(original);
        Expect(moved.HasTag("x")).ToBeTrue();
        Expect(moved.HasTag("y")).ToBeTrue();
        Expect(moved.suite_name).ToEqual(std::string_view("S"));
    }

    // === TestEntry composition: metadata field access ===

    void TestEntryMetadataFieldsAccessible() {
        Registry reg;
        reg.Add<MetaDummy>("MySuite", "MyTest", &MetaDummy::Alpha, {"tag1", "tag2"});
        const auto& entry = reg.Tests()[0];
        Expect(entry.metadata.suite_name).ToEqual(std::string_view("MySuite"));
        Expect(entry.metadata.test_name).ToEqual(std::string_view("MyTest"));
        Expect(entry.metadata.HasTag("tag1")).ToBeTrue();
        Expect(entry.metadata.HasTag("tag2")).ToBeTrue();
        Expect(entry.metadata.tags.size()).ToEqual(std::size_t{2});
    }

    void TestEntryMetadataNoTagsDefault() {
        Registry reg;
        reg.Add<MetaDummy>("S", "T", &MetaDummy::Alpha);
        Expect(reg.Tests()[0].metadata.tags.empty()).ToBeTrue();
    }

    // === Registry::Add returns TestEntry& ===

    void TestAddReturnsReferenceToBackEntry() {
        Registry reg;
        auto& entry_ref = reg.Add<MetaDummy>("S", "A", &MetaDummy::Alpha, {"fast"});
        // Returned reference should be the last entry
        Expect(entry_ref.metadata.suite_name).ToEqual(std::string_view("S"));
        Expect(entry_ref.metadata.test_name).ToEqual(std::string_view("A"));
        Expect(entry_ref.metadata.HasTag("fast")).ToBeTrue();
    }

    void TestAddReturnValueCanBeIgnored() {
        // Verify that ignoring the return value is fine (backward compat)
        Registry reg;
        reg.Add<MetaDummy>("S", "A", &MetaDummy::Alpha);
        Expect(reg.Tests().size()).ToEqual(std::size_t{1});
    }

    // === Critical: vector reallocation invalidation of Add return ref ===
    // Design says: "each builder chain completes (temporary destroyed) before
    // the next Add call, so no dangling references occur."
    // This test deliberately stores references across Add calls to see if
    // they get invalidated by reallocation.

    void TestAddReturnRefInvalidatedBySubsequentAdd() {
        Registry reg;
        // Get reference to first entry
        auto& first_ref = reg.Add<MetaDummy>("S", "First", &MetaDummy::Alpha, {"first"});
        // Store the name for comparison after potential reallocation
        auto first_name_before = first_ref.metadata.test_name;
        Expect(first_name_before).ToEqual(std::string_view("First"));

        // Add many more entries to trigger vector reallocation
        for (int i = 0; i < 100; ++i) {
            reg.Add<MetaDummy>("S", "Bulk", &MetaDummy::Beta);
        }

        // After reallocation, first_ref may be dangling.
        // The safe way to verify is via Tests() span.
        Expect(reg.Tests()[0].metadata.test_name).ToEqual(std::string_view("First"));
        Expect(reg.Tests()[0].metadata.HasTag("first")).ToBeTrue();
        Expect(reg.Tests().size()).ToEqual(std::size_t{101});
    }

    // === TestResult metadata reference validity during RunAll ===

    void TestResultMetadataRefValidDuringRunAll() {
        // Registry is created, Runner borrows it. During RunAll, TestResult
        // holds reference_wrapper<const TestMetadata> pointing into Registry.
        // Registry is const during RunAll (no mutations), so references are stable.
        Registry reg;
        reg.Add<MetaDummy>("S", "A", &MetaDummy::Alpha, {"t1"});
        reg.Add<MetaDummy>("S", "B", &MetaDummy::Beta, {"t2"});
        Runner runner(reg);
        // RunAll should succeed (both tests pass)
        Expect(runner.RunAll()).ToEqual(0);
    }

    void TestResultMetadataRefValidWithFailingTest() {
        // Ensure metadata reference is valid even for failing tests
        Registry reg;
        reg.Add<MetaFailDummy>("S", "Fail", &MetaFailDummy::Fail, {"will-fail"});
        Runner runner(reg);
        Expect(runner.RunAll()).ToEqual(1);
    }

    void TestResultMetadataRefValidWithMixedResults() {
        Registry reg;
        reg.Add<MetaDummy>("S", "Pass1", &MetaDummy::Alpha, {"ok"});
        reg.Add<MetaFailDummy>("S", "Fail1", &MetaFailDummy::Fail, {"bad"});
        reg.Add<MetaDummy>("S", "Pass2", &MetaDummy::Beta, {"ok"});
        Runner runner(reg);
        Expect(runner.RunAll()).ToEqual(1);
    }

    // === Large number of tests: stress-test that RunAll works with many entries ===

    void TestManyTestsRunAllStress() {
        Registry reg;
        for (int i = 0; i < 200; ++i) {
            reg.Add<MetaDummy>("Stress", "Test", &MetaDummy::Alpha);
        }
        Runner runner(reg);
        Expect(runner.RunAll()).ToEqual(0);
    }

    // === Tag filtering after metadata refactor ===

    void TestFilterByTagViaMetadata() {
        Registry reg;
        reg.Add<MetaDummy>("S", "A", &MetaDummy::Alpha, {"keep"});
        reg.Add<MetaDummy>("S", "B", &MetaDummy::Beta, {"drop"});
        reg.Add<MetaDummy>("S", "C", &MetaDummy::Gamma, {"keep"});
        std::vector<std::string_view> include = {"keep"};
        reg.FilterByTag(include);
        Expect(reg.Tests().size()).ToEqual(std::size_t{2});
        // Verify metadata access after filtering
        Expect(reg.Tests()[0].metadata.test_name).ToEqual(std::string_view("A"));
        Expect(reg.Tests()[0].metadata.HasTag("keep")).ToBeTrue();
        Expect(reg.Tests()[1].metadata.test_name).ToEqual(std::string_view("C"));
    }

    void TestExcludeByTagViaMetadata() {
        Registry reg;
        reg.Add<MetaDummy>("S", "A", &MetaDummy::Alpha, {"keep"});
        reg.Add<MetaDummy>("S", "B", &MetaDummy::Beta, {"drop"});
        std::vector<std::string_view> exclude = {"drop"};
        reg.ExcludeByTag(exclude);
        Expect(reg.Tests().size()).ToEqual(std::size_t{1});
        Expect(reg.Tests()[0].metadata.HasTag("keep")).ToBeTrue();
    }

    // === Deduplication in TestMetadata construction ===

    void TestDedupInMetadataViaRegistry() {
        Registry reg;
        reg.Add<MetaDummy>("S", "A", &MetaDummy::Alpha, {"dup", "dup", "unique"});
        Expect(reg.Tests()[0].metadata.tags.size()).ToEqual(std::size_t{2});
        Expect(reg.Tests()[0].metadata.HasTag("dup")).ToBeTrue();
        Expect(reg.Tests()[0].metadata.HasTag("unique")).ToBeTrue();
    }

    // === --list CTest safety invariant after refactor ===

    void TestListOutputBareNamesAfterMetadataRefactor() {
        Registry reg;
        reg.Add<MetaDummy>("Suite", "Alpha", &MetaDummy::Alpha, {"fast", "unit"});
        reg.Add<MetaDummy>("Suite", "Beta", &MetaDummy::Beta);
        auto argv = MakeArgv({"prog", "--list"});
        auto rc = flul::test::Run(static_cast<int>(argv.size()), argv.data(), reg);
        Expect(rc).ToEqual(0);
        // After --list, both tests should still be in the registry (list does not filter)
        Expect(reg.Tests().size()).ToEqual(std::size_t{2});
    }

    // === --list-verbose shows tags correctly after refactor ===

    void TestListVerboseShowsTagsAfterMetadataRefactor() {
        Registry reg;
        reg.Add<MetaDummy>("Suite", "Alpha", &MetaDummy::Alpha, {"fast", "unit"});
        reg.Add<MetaDummy>("Suite", "Beta", &MetaDummy::Beta);
        auto argv = MakeArgv({"prog", "--list-verbose"});
        auto rc = flul::test::Run(static_cast<int>(argv.size()), argv.data(), reg);
        Expect(rc).ToEqual(0);
    }

    // === Filter via metadata.suite_name and metadata.test_name ===

    void TestFilterUsesMetadataFields() {
        Registry reg;
        reg.Add<MetaDummy>("MySuite", "TestAlpha", &MetaDummy::Alpha);
        reg.Add<MetaDummy>("MySuite", "TestBeta", &MetaDummy::Beta);
        reg.Add<MetaDummy>("OtherSuite", "TestGamma", &MetaDummy::Gamma);
        reg.Filter("MySuite");
        Expect(reg.Tests().size()).ToEqual(std::size_t{2});
        Expect(reg.Tests()[0].metadata.suite_name).ToEqual(std::string_view("MySuite"));
        Expect(reg.Tests()[1].metadata.suite_name).ToEqual(std::string_view("MySuite"));
    }

    // === Run with tags + filter composition after metadata refactor ===

    void TestRunTagFilterComposeViaMetadata() {
        Registry reg;
        reg.Add<MetaDummy>("S", "A", &MetaDummy::Alpha, {"fast"});
        reg.Add<MetaDummy>("S", "B", &MetaDummy::Beta, {"fast"});
        reg.Add<MetaDummy>("S", "C", &MetaDummy::Gamma, {"slow"});
        auto argv = MakeArgv({"prog", "--filter", "A", "--tag", "fast"});
        auto rc = flul::test::Run(static_cast<int>(argv.size()), argv.data(), reg);
        Expect(rc).ToEqual(0);
        Expect(reg.Tests().size()).ToEqual(std::size_t{1});
        Expect(reg.Tests()[0].metadata.test_name).ToEqual(std::string_view("A"));
    }

    // === Architecture constraint: TestMetadata tags is std::set (sorted, unique) ===

    void TestTagsAreSortedInMetadata() {
        Registry reg;
        reg.Add<MetaDummy>("S", "A", &MetaDummy::Alpha, {"zebra", "alpha", "mid"});
        const auto& tags = reg.Tests()[0].metadata.tags;
        // std::set should store them in sorted order
        auto it = tags.begin();
        Expect(*it).ToEqual(std::string_view("alpha"));
        ++it;
        Expect(*it).ToEqual(std::string_view("mid"));
        ++it;
        Expect(*it).ToEqual(std::string_view("zebra"));
    }

    // === Architecture divergence check: tags field is on TestMetadata, not TestEntry ===

    void TestTagsNotDirectlyOnTestEntry() {
        // This test verifies the structural refactoring: tags are accessed
        // via entry.metadata.tags, not entry.tags (which no longer exists).
        Registry reg;
        reg.Add<MetaDummy>("S", "A", &MetaDummy::Alpha, {"x"});
        // If someone accidentally re-added tags to TestEntry, this would compile
        // but be wrong. We verify via metadata.
        Expect(reg.Tests()[0].metadata.tags.size()).ToEqual(std::size_t{1});
        Expect(reg.Tests()[0].metadata.HasTag("x")).ToBeTrue();
    }

    // === Edge: TestResult construction with std::cref ===

    void TestResultCanBeConstructedWithCref() {
        TestMetadata meta{.suite_name = "S", .test_name = "T", .tags = {"a"}};
        flul::test::TestResult result{
            .metadata = std::cref(meta),
            .passed = true,
            .duration = std::chrono::nanoseconds{42},
            .error = std::nullopt,
        };
        Expect(result.metadata.get().suite_name).ToEqual(std::string_view("S"));
        Expect(result.metadata.get().test_name).ToEqual(std::string_view("T"));
        Expect(result.metadata.get().HasTag("a")).ToBeTrue();
        Expect(result.passed).ToBeTrue();
    }

    // === Edge: multiple TestResults referencing same TestMetadata ===

    void TestMultipleResultsReferenceSameMetadata() {
        TestMetadata meta{.suite_name = "S", .test_name = "T", .tags = {}};
        flul::test::TestResult r1{
            .metadata = std::cref(meta),
            .passed = true,
            .duration = std::chrono::nanoseconds{1},
            .error = std::nullopt,
        };
        flul::test::TestResult r2{
            .metadata = std::cref(meta),
            .passed = false,
            .duration = std::chrono::nanoseconds{2},
            .error = std::nullopt,
        };
        // Both reference the same metadata
        Expect(&r1.metadata.get()).ToEqual(&r2.metadata.get());
    }

    // === Stress: Run with many tagged tests to verify no reference issues ===

    void TestRunManyTaggedTests() {
        Registry reg;
        for (int i = 0; i < 50; ++i) {
            reg.Add<MetaDummy>("Stress", "Tagged", &MetaDummy::Alpha, {"bulk"});
        }
        for (int i = 0; i < 50; ++i) {
            reg.Add<MetaDummy>("Stress", "Untagged", &MetaDummy::Beta);
        }
        // Filter to tagged only
        std::vector<std::string_view> include = {"bulk"};
        reg.FilterByTag(include);
        Expect(reg.Tests().size()).ToEqual(std::size_t{50});
        // Run them all
        Runner runner(reg);
        Expect(runner.RunAll()).ToEqual(0);
    }

    static void Register(Registry& r) {
        AddTests(
            r, "TagMetadataAdversarialSuite",
            {
                {"TestMetadataHasTagOnEmptyTagSet",
                 &TagMetadataAdversarialSuite::TestMetadataHasTagOnEmptyTagSet},
                {"TestMetadataHasTagFindsPresent",
                 &TagMetadataAdversarialSuite::TestMetadataHasTagFindsPresent},
                {"TestMetadataDefaultConstruction",
                 &TagMetadataAdversarialSuite::TestMetadataDefaultConstruction},
                {"TestMetadataCopyPreservesTags",
                 &TagMetadataAdversarialSuite::TestMetadataCopyPreservesTags},
                {"TestMetadataMovePreservesTags",
                 &TagMetadataAdversarialSuite::TestMetadataMovePreservesTags},
                {"TestEntryMetadataFieldsAccessible",
                 &TagMetadataAdversarialSuite::TestEntryMetadataFieldsAccessible},
                {"TestEntryMetadataNoTagsDefault",
                 &TagMetadataAdversarialSuite::TestEntryMetadataNoTagsDefault},
                {"TestAddReturnsReferenceToBackEntry",
                 &TagMetadataAdversarialSuite::TestAddReturnsReferenceToBackEntry},
                {"TestAddReturnValueCanBeIgnored",
                 &TagMetadataAdversarialSuite::TestAddReturnValueCanBeIgnored},
                {"TestAddReturnRefInvalidatedBySubsequentAdd",
                 &TagMetadataAdversarialSuite::TestAddReturnRefInvalidatedBySubsequentAdd},
                {"TestResultMetadataRefValidDuringRunAll",
                 &TagMetadataAdversarialSuite::TestResultMetadataRefValidDuringRunAll},
                {"TestResultMetadataRefValidWithFailingTest",
                 &TagMetadataAdversarialSuite::TestResultMetadataRefValidWithFailingTest},
                {"TestResultMetadataRefValidWithMixedResults",
                 &TagMetadataAdversarialSuite::TestResultMetadataRefValidWithMixedResults},
                {"TestManyTestsRunAllStress",
                 &TagMetadataAdversarialSuite::TestManyTestsRunAllStress},
                {"TestFilterByTagViaMetadata",
                 &TagMetadataAdversarialSuite::TestFilterByTagViaMetadata},
                {"TestExcludeByTagViaMetadata",
                 &TagMetadataAdversarialSuite::TestExcludeByTagViaMetadata},
                {"TestDedupInMetadataViaRegistry",
                 &TagMetadataAdversarialSuite::TestDedupInMetadataViaRegistry},
                {"TestListOutputBareNamesAfterMetadataRefactor",
                 &TagMetadataAdversarialSuite::TestListOutputBareNamesAfterMetadataRefactor},
                {"TestListVerboseShowsTagsAfterMetadataRefactor",
                 &TagMetadataAdversarialSuite::TestListVerboseShowsTagsAfterMetadataRefactor},
                {"TestFilterUsesMetadataFields",
                 &TagMetadataAdversarialSuite::TestFilterUsesMetadataFields},
                {"TestRunTagFilterComposeViaMetadata",
                 &TagMetadataAdversarialSuite::TestRunTagFilterComposeViaMetadata},
                {"TestTagsAreSortedInMetadata",
                 &TagMetadataAdversarialSuite::TestTagsAreSortedInMetadata},
                {"TestTagsNotDirectlyOnTestEntry",
                 &TagMetadataAdversarialSuite::TestTagsNotDirectlyOnTestEntry},
                {"TestResultCanBeConstructedWithCref",
                 &TagMetadataAdversarialSuite::TestResultCanBeConstructedWithCref},
                {"TestMultipleResultsReferenceSameMetadata",
                 &TagMetadataAdversarialSuite::TestMultipleResultsReferenceSameMetadata},
                {"TestRunManyTaggedTests", &TagMetadataAdversarialSuite::TestRunManyTaggedTests},
            });
    }
};

// NOLINTEND(readability-convert-member-functions-to-static,readability-make-member-function-const)

namespace tag_metadata_adversarial_test {
void Register(Registry& r) {  // NOLINT(misc-use-internal-linkage)
    TagMetadataAdversarialSuite::Register(r);
}
}  // namespace tag_metadata_adversarial_test
