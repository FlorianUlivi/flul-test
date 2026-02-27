#include "flul/test/expect.hpp"
#include "flul/test/registry.hpp"
#include "flul/test/run.hpp"

using flul::test::Expect;
using flul::test::Registry;
using flul::test::Suite;

namespace {

// NOLINTBEGIN(readability-convert-member-functions-to-static)

class TagDummySuite : public Suite<TagDummySuite> {
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

}  // namespace

// NOLINTBEGIN(readability-convert-member-functions-to-static,readability-make-member-function-const)

class TagSuite : public Suite<TagSuite> {
   public:
    // --- TestEntry::HasTag ---

    void TestHasTagReturnsTrueForPresentTag() {
        Registry reg;
        reg.Add<TagDummySuite>("S", "A", &TagDummySuite::Alpha, {"fast", "math"});
        Expect(reg.Tests()[0].HasTag("fast")).ToBeTrue();
        Expect(reg.Tests()[0].HasTag("math")).ToBeTrue();
    }

    void TestHasTagReturnsFalseForAbsentTag() {
        Registry reg;
        reg.Add<TagDummySuite>("S", "A", &TagDummySuite::Alpha, {"fast"});
        Expect(reg.Tests()[0].HasTag("slow")).ToBeFalse();
    }

    void TestHasTagReturnsFalseWhenNoTags() {
        Registry reg;
        reg.Add<TagDummySuite>("S", "A", &TagDummySuite::Alpha);
        Expect(reg.Tests()[0].HasTag("fast")).ToBeFalse();
        Expect(reg.Tests()[0].tags.empty()).ToBeTrue();
    }

    // --- Registry::Add with tags ---

    void TestAddStoresTags() {
        Registry reg;
        reg.Add<TagDummySuite>("S", "A", &TagDummySuite::Alpha, {"x", "y"});
        Expect(reg.Tests()[0].tags.size()).ToEqual(std::size_t{2});
    }

    void TestAddDefaultTagsEmpty() {
        Registry reg;
        reg.Add<TagDummySuite>("S", "A", &TagDummySuite::Alpha);
        Expect(reg.Tests()[0].tags.empty()).ToBeTrue();
    }

    // --- AddTests with tags ---

    void TestAddTestsPassesTagsToAllTests() {
        Registry reg;
        TagDummySuite::AddTests(reg, "S",
                                {
                                    {"Alpha", &TagDummySuite::Alpha},
                                    {"Beta", &TagDummySuite::Beta},
                                },
                                {"unit", "fast"});
        Expect(reg.Tests().size()).ToEqual(std::size_t{2});
        Expect(reg.Tests()[0].HasTag("unit")).ToBeTrue();
        Expect(reg.Tests()[1].HasTag("unit")).ToBeTrue();
    }

    void TestAddTestsDefaultTagsEmpty() {
        Registry reg;
        TagDummySuite::AddTests(reg, "S",
                                {
                                    {"Alpha", &TagDummySuite::Alpha},
                                });
        Expect(reg.Tests()[0].tags.empty()).ToBeTrue();
    }

    // --- Registry::FilterByTag ---

    void TestFilterByTagKeepsMatchingTests() {
        Registry reg;
        reg.Add<TagDummySuite>("S", "Alpha", &TagDummySuite::Alpha, {"fast"});
        reg.Add<TagDummySuite>("S", "Beta", &TagDummySuite::Beta, {"slow"});
        reg.Add<TagDummySuite>("S", "Gamma", &TagDummySuite::Gamma, {"fast"});
        std::vector<std::string_view> include = {"fast"};
        reg.FilterByTag(include);
        Expect(reg.Tests().size()).ToEqual(std::size_t{2});
        Expect(reg.Tests()[0].test_name).ToEqual(std::string_view("Alpha"));
        Expect(reg.Tests()[1].test_name).ToEqual(std::string_view("Gamma"));
    }

    void TestFilterByTagOrSemantics() {
        Registry reg;
        reg.Add<TagDummySuite>("S", "Alpha", &TagDummySuite::Alpha, {"fast"});
        reg.Add<TagDummySuite>("S", "Beta", &TagDummySuite::Beta, {"slow"});
        reg.Add<TagDummySuite>("S", "Gamma", &TagDummySuite::Gamma);
        std::vector<std::string_view> include = {"fast", "slow"};
        reg.FilterByTag(include);
        Expect(reg.Tests().size()).ToEqual(std::size_t{2});
    }

    void TestFilterByTagEmptyIsNoOp() {
        Registry reg;
        reg.Add<TagDummySuite>("S", "Alpha", &TagDummySuite::Alpha, {"fast"});
        reg.Add<TagDummySuite>("S", "Beta", &TagDummySuite::Beta);
        std::vector<std::string_view> empty;
        reg.FilterByTag(empty);
        Expect(reg.Tests().size()).ToEqual(std::size_t{2});
    }

    // --- Registry::ExcludeByTag ---

    void TestExcludeByTagRemovesMatchingTests() {
        Registry reg;
        reg.Add<TagDummySuite>("S", "Alpha", &TagDummySuite::Alpha, {"fast"});
        reg.Add<TagDummySuite>("S", "Beta", &TagDummySuite::Beta, {"slow"});
        reg.Add<TagDummySuite>("S", "Gamma", &TagDummySuite::Gamma);
        std::vector<std::string_view> exclude = {"fast"};
        reg.ExcludeByTag(exclude);
        Expect(reg.Tests().size()).ToEqual(std::size_t{2});
        Expect(reg.Tests()[0].test_name).ToEqual(std::string_view("Beta"));
        Expect(reg.Tests()[1].test_name).ToEqual(std::string_view("Gamma"));
    }

    void TestExcludeByTagEmptyIsNoOp() {
        Registry reg;
        reg.Add<TagDummySuite>("S", "Alpha", &TagDummySuite::Alpha, {"fast"});
        std::vector<std::string_view> empty;
        reg.ExcludeByTag(empty);
        Expect(reg.Tests().size()).ToEqual(std::size_t{1});
    }

    void TestExcludeOverridesInclude() {
        Registry reg;
        reg.Add<TagDummySuite>("S", "Alpha", &TagDummySuite::Alpha, {"fast", "slow"});
        reg.Add<TagDummySuite>("S", "Beta", &TagDummySuite::Beta, {"fast"});
        // Include "fast", then exclude "slow" — Alpha matches both, should be excluded
        std::vector<std::string_view> include = {"fast"};
        std::vector<std::string_view> exclude = {"slow"};
        reg.FilterByTag(include);
        reg.ExcludeByTag(exclude);
        Expect(reg.Tests().size()).ToEqual(std::size_t{1});
        Expect(reg.Tests()[0].test_name).ToEqual(std::string_view("Beta"));
    }

    // --- Registry::ListVerbose ---

    void TestListVerboseNoTags() {
        Registry reg;
        reg.Add<TagDummySuite>("S", "Alpha", &TagDummySuite::Alpha);
        reg.ListVerbose();  // Should not crash; no bracket suffix
    }

    void TestListVerboseWithTags() {
        Registry reg;
        reg.Add<TagDummySuite>("S", "Alpha", &TagDummySuite::Alpha, {"fast", "math"});
        reg.ListVerbose();  // Should not crash; outputs [fast, math]
    }

    // --- Run() CLI flags ---

    void TestRunTagFlag() {
        Registry reg;
        reg.Add<TagDummySuite>("S", "Alpha", &TagDummySuite::Alpha, {"fast"});
        reg.Add<TagDummySuite>("S", "Beta", &TagDummySuite::Beta, {"slow"});
        auto argv = MakeArgv({"prog", "--tag", "fast"});
        Expect(flul::test::Run(static_cast<int>(argv.size()), argv.data(), reg)).ToEqual(0);
        Expect(reg.Tests().size()).ToEqual(std::size_t{1});
    }

    void TestRunTagFlagMissingArg() {
        Registry reg;
        auto argv = MakeArgv({"prog", "--tag"});
        Expect(flul::test::Run(static_cast<int>(argv.size()), argv.data(), reg)).ToEqual(1);
    }

    void TestRunExcludeTagFlag() {
        Registry reg;
        reg.Add<TagDummySuite>("S", "Alpha", &TagDummySuite::Alpha, {"fast"});
        reg.Add<TagDummySuite>("S", "Beta", &TagDummySuite::Beta, {"slow"});
        auto argv = MakeArgv({"prog", "--exclude-tag", "fast"});
        Expect(flul::test::Run(static_cast<int>(argv.size()), argv.data(), reg)).ToEqual(0);
        Expect(reg.Tests().size()).ToEqual(std::size_t{1});
    }

    void TestRunExcludeTagFlagMissingArg() {
        Registry reg;
        auto argv = MakeArgv({"prog", "--exclude-tag"});
        Expect(flul::test::Run(static_cast<int>(argv.size()), argv.data(), reg)).ToEqual(1);
    }

    void TestRunListVerboseFlag() {
        Registry reg;
        reg.Add<TagDummySuite>("S", "Alpha", &TagDummySuite::Alpha, {"fast"});
        auto argv = MakeArgv({"prog", "--list-verbose"});
        Expect(flul::test::Run(static_cast<int>(argv.size()), argv.data(), reg)).ToEqual(0);
    }

    void TestRunTagAndExcludeTagCompose() {
        Registry reg;
        reg.Add<TagDummySuite>("S", "Alpha", &TagDummySuite::Alpha, {"fast", "slow"});
        reg.Add<TagDummySuite>("S", "Beta", &TagDummySuite::Beta, {"fast"});
        // --tag fast, --exclude-tag slow: Alpha excluded (has slow), Beta kept
        auto argv = MakeArgv({"prog", "--tag", "fast", "--exclude-tag", "slow"});
        Expect(flul::test::Run(static_cast<int>(argv.size()), argv.data(), reg)).ToEqual(0);
        Expect(reg.Tests().size()).ToEqual(std::size_t{1});
        Expect(reg.Tests()[0].test_name).ToEqual(std::string_view("Beta"));
    }

    void TestRunFilterAndTagCompose() {
        Registry reg;
        reg.Add<TagDummySuite>("S", "Alpha", &TagDummySuite::Alpha, {"fast"});
        reg.Add<TagDummySuite>("S", "Beta", &TagDummySuite::Beta, {"fast"});
        // --filter Alpha leaves only Alpha, then --tag fast keeps it
        auto argv = MakeArgv({"prog", "--filter", "Alpha", "--tag", "fast"});
        Expect(flul::test::Run(static_cast<int>(argv.size()), argv.data(), reg)).ToEqual(0);
        Expect(reg.Tests().size()).ToEqual(std::size_t{1});
        Expect(reg.Tests()[0].test_name).ToEqual(std::string_view("Alpha"));
    }

    void TestRunMultipleTagFlags() {
        Registry reg;
        reg.Add<TagDummySuite>("S", "Alpha", &TagDummySuite::Alpha, {"fast"});
        reg.Add<TagDummySuite>("S", "Beta", &TagDummySuite::Beta, {"slow"});
        reg.Add<TagDummySuite>("S", "Gamma", &TagDummySuite::Gamma);
        // --tag fast --tag slow => OR: Alpha + Beta kept, Gamma excluded
        auto argv = MakeArgv({"prog", "--tag", "fast", "--tag", "slow"});
        Expect(flul::test::Run(static_cast<int>(argv.size()), argv.data(), reg)).ToEqual(0);
        Expect(reg.Tests().size()).ToEqual(std::size_t{2});
    }

    static void Register(Registry& r) {
        AddTests(
            r, "TagSuite",
            {
                {"TestHasTagReturnsTrueForPresentTag",
                 &TagSuite::TestHasTagReturnsTrueForPresentTag},
                {"TestHasTagReturnsFalseForAbsentTag",
                 &TagSuite::TestHasTagReturnsFalseForAbsentTag},
                {"TestHasTagReturnsFalseWhenNoTags", &TagSuite::TestHasTagReturnsFalseWhenNoTags},
                {"TestAddStoresTags", &TagSuite::TestAddStoresTags},
                {"TestAddDefaultTagsEmpty", &TagSuite::TestAddDefaultTagsEmpty},
                {"TestAddTestsPassesTagsToAllTests", &TagSuite::TestAddTestsPassesTagsToAllTests},
                {"TestAddTestsDefaultTagsEmpty", &TagSuite::TestAddTestsDefaultTagsEmpty},
                {"TestFilterByTagKeepsMatchingTests", &TagSuite::TestFilterByTagKeepsMatchingTests},
                {"TestFilterByTagOrSemantics", &TagSuite::TestFilterByTagOrSemantics},
                {"TestFilterByTagEmptyIsNoOp", &TagSuite::TestFilterByTagEmptyIsNoOp},
                {"TestExcludeByTagRemovesMatchingTests",
                 &TagSuite::TestExcludeByTagRemovesMatchingTests},
                {"TestExcludeByTagEmptyIsNoOp", &TagSuite::TestExcludeByTagEmptyIsNoOp},
                {"TestExcludeOverridesInclude", &TagSuite::TestExcludeOverridesInclude},
                {"TestListVerboseNoTags", &TagSuite::TestListVerboseNoTags},
                {"TestListVerboseWithTags", &TagSuite::TestListVerboseWithTags},
                {"TestRunTagFlag", &TagSuite::TestRunTagFlag},
                {"TestRunTagFlagMissingArg", &TagSuite::TestRunTagFlagMissingArg},
                {"TestRunExcludeTagFlag", &TagSuite::TestRunExcludeTagFlag},
                {"TestRunExcludeTagFlagMissingArg", &TagSuite::TestRunExcludeTagFlagMissingArg},
                {"TestRunListVerboseFlag", &TagSuite::TestRunListVerboseFlag},
                {"TestRunTagAndExcludeTagCompose", &TagSuite::TestRunTagAndExcludeTagCompose},
                {"TestRunFilterAndTagCompose", &TagSuite::TestRunFilterAndTagCompose},
                {"TestRunMultipleTagFlags", &TagSuite::TestRunMultipleTagFlags},
            });
    }
};

// NOLINTEND(readability-convert-member-functions-to-static,readability-make-member-function-const)

namespace tag_test {
void Register(Registry& r) {  // NOLINT(misc-use-internal-linkage)
    TagSuite::Register(r);
}
}  // namespace tag_test
