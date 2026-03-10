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

using flul::test::Expect;
using flul::test::Registry;
using flul::test::Suite;

namespace {

// NOLINTBEGIN(readability-convert-member-functions-to-static)

class TagValDummy : public Suite<TagValDummy> {
   public:
    void Noop() {}
};

// NOLINTEND(readability-convert-member-functions-to-static)

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

auto CaptureDiagnostic(auto fn) -> std::pair<bool, std::string> {
    int pipefd[2] = {};
    pipe(pipefd);

    pid_t pid = fork();  // NOLINT(misc-const-correctness)
    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        fn();
        _Exit(0);
    }

    close(pipefd[1]);
    std::string captured;
    char buf[512] = {};
    ssize_t n = 0;
    while ((n = read(pipefd[0], buf, sizeof(buf))) > 0) {
        captured.append(buf, static_cast<std::size_t>(n));
    }
    close(pipefd[0]);
    int status = 0;
    waitpid(pid, &status, 0);

    // NOLINTBEGIN(misc-const-correctness)
    bool died = WIFSIGNALED(status) ||                            // NOLINT(hicpp-signed-bitwise)
                (WIFEXITED(status) && WEXITSTATUS(status) != 0);  // NOLINT(hicpp-signed-bitwise)
    // NOLINTEND(misc-const-correctness)
    return {died, captured};
}

}  // namespace

// NOLINTBEGIN(readability-convert-member-functions-to-static,readability-make-member-function-const)

class TagValidationAdversarialSuite : public Suite<TagValidationAdversarialSuite> {
   public:
    // ========================================================================
    // Single-char valid tags: boundary characters just inside the allowlist
    // ========================================================================

    void TestSingleCharLowercaseA() {
        Registry reg;
        reg.Add<TagValDummy>("S", "T", &TagValDummy::Noop, {"a"});
        Expect(reg.Tests()[0].metadata.HasTag("a")).ToBeTrue();
    }

    void TestSingleCharLowercaseZ() {
        Registry reg;
        reg.Add<TagValDummy>("S", "T", &TagValDummy::Noop, {"z"});
        Expect(reg.Tests()[0].metadata.HasTag("z")).ToBeTrue();
    }

    void TestSingleCharUppercaseA() {
        Registry reg;
        reg.Add<TagValDummy>("S", "T", &TagValDummy::Noop, {"A"});
        Expect(reg.Tests()[0].metadata.HasTag("A")).ToBeTrue();
    }

    void TestSingleCharUppercaseZ() {
        Registry reg;
        reg.Add<TagValDummy>("S", "T", &TagValDummy::Noop, {"Z"});
        Expect(reg.Tests()[0].metadata.HasTag("Z")).ToBeTrue();
    }

    void TestSingleCharDigit0() {
        Registry reg;
        reg.Add<TagValDummy>("S", "T", &TagValDummy::Noop, {"0"});
        Expect(reg.Tests()[0].metadata.HasTag("0")).ToBeTrue();
    }

    void TestSingleCharDigit9() {
        Registry reg;
        reg.Add<TagValDummy>("S", "T", &TagValDummy::Noop, {"9"});
        Expect(reg.Tests()[0].metadata.HasTag("9")).ToBeTrue();
    }

    void TestSingleCharUnderscore() {
        Registry reg;
        reg.Add<TagValDummy>("S", "T", &TagValDummy::Noop, {"_"});
        Expect(reg.Tests()[0].metadata.HasTag("_")).ToBeTrue();
    }

    void TestSingleCharHyphen() {
        Registry reg;
        reg.Add<TagValDummy>("S", "T", &TagValDummy::Noop, {"-"});
        Expect(reg.Tests()[0].metadata.HasTag("-")).ToBeTrue();
    }

    // ========================================================================
    // All-hyphens and all-underscores tags
    // ========================================================================

    void TestAllHyphens() {
        Registry reg;
        reg.Add<TagValDummy>("S", "T", &TagValDummy::Noop, {"--"});
        Expect(reg.Tests()[0].metadata.HasTag("--")).ToBeTrue();
    }

    void TestAllUnderscores() {
        Registry reg;
        reg.Add<TagValDummy>("S", "T", &TagValDummy::Noop, {"___"});
        Expect(reg.Tests()[0].metadata.HasTag("___")).ToBeTrue();
    }

    void TestMixedHyphensUnderscores() {
        Registry reg;
        reg.Add<TagValDummy>("S", "T", &TagValDummy::Noop, {"_-_-"});
        Expect(reg.Tests()[0].metadata.HasTag("_-_-")).ToBeTrue();
    }

    // ========================================================================
    // Single-char invalid tags: boundary characters just outside the allowlist
    // ========================================================================

    void TestSingleCharSpace() {
        Expect(DiesOnTerminate([] {
            Registry reg;
            reg.Add<TagValDummy>("S", "T", &TagValDummy::Noop, {" "});
        })).ToBeTrue();
    }

    void TestSingleCharAt() {
        Expect(DiesOnTerminate([] {
            Registry reg;
            reg.Add<TagValDummy>("S", "T", &TagValDummy::Noop, {"@"});
        })).ToBeTrue();
    }

    void TestSingleCharOpenBracket() {
        Expect(DiesOnTerminate([] {
            Registry reg;
            reg.Add<TagValDummy>("S", "T", &TagValDummy::Noop, {"["});
        })).ToBeTrue();
    }

    void TestSingleCharCloseBracket() {
        Expect(DiesOnTerminate([] {
            Registry reg;
            reg.Add<TagValDummy>("S", "T", &TagValDummy::Noop, {"]"});
        })).ToBeTrue();
    }

    void TestSingleCharSemicolon() {
        Expect(DiesOnTerminate([] {
            Registry reg;
            reg.Add<TagValDummy>("S", "T", &TagValDummy::Noop, {";"});
        })).ToBeTrue();
    }

    void TestSingleCharSlash() {
        Expect(DiesOnTerminate([] {
            Registry reg;
            reg.Add<TagValDummy>("S", "T", &TagValDummy::Noop, {"/"});
        })).ToBeTrue();
    }

    void TestSingleCharDot() {
        Expect(DiesOnTerminate([] {
            Registry reg;
            reg.Add<TagValDummy>("S", "T", &TagValDummy::Noop, {"."});
        })).ToBeTrue();
    }

    void TestSingleCharColon() {
        Expect(DiesOnTerminate([] {
            Registry reg;
            reg.Add<TagValDummy>("S", "T", &TagValDummy::Noop, {":"});
        })).ToBeTrue();
    }

    void TestSingleCharComma() {
        Expect(DiesOnTerminate([] {
            Registry reg;
            reg.Add<TagValDummy>("S", "T", &TagValDummy::Noop, {","});
        })).ToBeTrue();
    }

    // ========================================================================
    // Tags with whitespace variants
    // ========================================================================

    void TestTagWithNewline() {
        Expect(DiesOnTerminate([] {
            Registry reg;
            reg.Add<TagValDummy>("S", "T", &TagValDummy::Noop, {"tag\n"});
        })).ToBeTrue();
    }

    void TestTagWithTab() {
        Expect(DiesOnTerminate([] {
            Registry reg;
            reg.Add<TagValDummy>("S", "T", &TagValDummy::Noop, {"tag\t"});
        })).ToBeTrue();
    }

    void TestTagWithCarriageReturn() {
        Expect(DiesOnTerminate([] {
            Registry reg;
            reg.Add<TagValDummy>("S", "T", &TagValDummy::Noop, {"tag\r"});
        })).ToBeTrue();
    }

    void TestTagWithEmbeddedSpace() {
        Expect(DiesOnTerminate([] {
            Registry reg;
            reg.Add<TagValDummy>("S", "T", &TagValDummy::Noop, {"two words"});
        })).ToBeTrue();
    }

    // ========================================================================
    // Tags with null bytes (tricky: string_view can contain embedded nulls)
    // ========================================================================

    void TestTagWithEmbeddedNull() {
        Expect(DiesOnTerminate([] {
            Registry reg;
            const std::string_view tag_with_null("ab\0cd", 5);
            reg.Add<TagValDummy>("S", "T", &TagValDummy::Noop, {tag_with_null});
        })).ToBeTrue();
    }

    // ========================================================================
    // Non-ASCII bytes (high bytes, e.g. 0x80, 0xFF)
    // ========================================================================

    void TestTagWithHighByte0x80() {
        Expect(DiesOnTerminate([] {
            Registry reg;
            reg.Add<TagValDummy>("S", "T", &TagValDummy::Noop, {"\x80"});
        })).ToBeTrue();
    }

    void TestTagWithHighByteFF() {
        Expect(DiesOnTerminate([] {
            Registry reg;
            reg.Add<TagValDummy>("S", "T", &TagValDummy::Noop, {"\xFF"});
        })).ToBeTrue();
    }

    void TestTagWithUtf8Sequence() {
        Expect(DiesOnTerminate([] {
            Registry reg;
            // UTF-8 encoded e-acute
            reg.Add<TagValDummy>("S", "T", &TagValDummy::Noop, {"\xC3\xA9"});
        })).ToBeTrue();
    }

    // ========================================================================
    // Very long tags (stress test)
    // ========================================================================

    void TestVeryLongValidTag() {
        Registry reg;
        const std::string long_tag(10000, 'x');
        reg.Add<TagValDummy>("S", "T", &TagValDummy::Noop, {long_tag});
        Expect(reg.Tests()[0].metadata.HasTag(long_tag)).ToBeTrue();
    }

    void TestVeryLongInvalidTag() {
        Expect(DiesOnTerminate([] {
            Registry reg;
            std::string long_tag(10000, 'x');
            long_tag[9999] = ' ';
            reg.Add<TagValDummy>("S", "T", &TagValDummy::Noop, {long_tag});
        })).ToBeTrue();
    }

    // ========================================================================
    // Mix of valid and invalid tags in same Add call
    // ========================================================================

    void TestValidThenInvalidAborts() {
        Expect(DiesOnTerminate([] {
            Registry reg;
            reg.Add<TagValDummy>("S", "T", &TagValDummy::Noop, {"valid", "inv@lid"});
        })).ToBeTrue();
    }

    void TestInvalidThenValidAborts() {
        Expect(DiesOnTerminate([] {
            Registry reg;
            reg.Add<TagValDummy>("S", "T", &TagValDummy::Noop, {"inv@lid", "valid"});
        })).ToBeTrue();
    }

    void TestMultipleInvalidTagsAborts() {
        Expect(DiesOnTerminate([] {
            Registry reg;
            reg.Add<TagValDummy>("S", "T", &TagValDummy::Noop, {"b@d", "w[rse"});
        })).ToBeTrue();
    }

    // ========================================================================
    // Dedup runs before validation: duplicate invalid tags deduped then abort
    // ========================================================================

    void TestDuplicateInvalidTagDedupedBeforeValidation() {
        // Two copies of the same invalid tag; dedup reduces to one, then validation catches it
        auto [died, output] = CaptureDiagnostic([] {
            Registry reg;
            reg.Add<TagValDummy>("S", "T", &TagValDummy::Noop, {"b@d", "b@d"});
        });
        Expect(died).ToBeTrue();
        // Should still mention the invalid tag in diagnostic
        Expect(output.contains("b@d")).ToBeTrue();
    }

    // ========================================================================
    // Diagnostic message content verification
    // ========================================================================

    void TestDiagnosticContainsErrorKeyword() {
        auto [died, output] = CaptureDiagnostic([] {
            Registry reg;
            reg.Add<TagValDummy>("S", "T", &TagValDummy::Noop, {"b@d"});
        });
        Expect(died).ToBeTrue();
        Expect(output.contains("error")).ToBeTrue();
    }

    void TestDiagnosticContainsFlulTestPrefix() {
        auto [died, output] = CaptureDiagnostic([] {
            Registry reg;
            reg.Add<TagValDummy>("S", "T", &TagValDummy::Noop, {"b@d"});
        });
        Expect(died).ToBeTrue();
        Expect(output.contains("[flul-test]")).ToBeTrue();
    }

    void TestDiagnosticContainsOffendingTag() {
        auto [died, output] = CaptureDiagnostic([] {
            Registry reg;
            reg.Add<TagValDummy>("MySuite", "MyTest", &TagValDummy::Noop, {"sp ace"});
        });
        Expect(died).ToBeTrue();
        Expect(output.contains("sp ace")).ToBeTrue();
    }

    void TestDiagnosticContainsSuiteAndTestName() {
        auto [died, output] = CaptureDiagnostic([] {
            Registry reg;
            reg.Add<TagValDummy>("MySuite", "MyTest", &TagValDummy::Noop, {"in valid"});
        });
        Expect(died).ToBeTrue();
        Expect(output.contains("MySuite::MyTest")).ToBeTrue();
    }

    void TestDiagnosticContainsAllowlistPattern() {
        auto [died, output] = CaptureDiagnostic([] {
            Registry reg;
            reg.Add<TagValDummy>("S", "T", &TagValDummy::Noop, {"b@d"});
        });
        Expect(died).ToBeTrue();
        Expect(output.contains("[a-zA-Z0-9_-]+")).ToBeTrue();
    }

    // ========================================================================
    // Valid tag does NOT abort (sanity checks for tags near boundary)
    // ========================================================================

    void TestMixedCaseAlphanumericTag() {
        Registry reg;
        reg.Add<TagValDummy>("S", "T", &TagValDummy::Noop, {"Fast2Win_now-v3"});
        Expect(reg.Tests()[0].metadata.HasTag("Fast2Win_now-v3")).ToBeTrue();
    }

    void TestTagOfAllDigits() {
        Registry reg;
        reg.Add<TagValDummy>("S", "T", &TagValDummy::Noop, {"12345"});
        Expect(reg.Tests()[0].metadata.HasTag("12345")).ToBeTrue();
    }

    void TestTagOfAllUppercase() {
        Registry reg;
        reg.Add<TagValDummy>("S", "T", &TagValDummy::Noop, {"ABCDEFGHIJKLMNOPQRSTUVWXYZ"});
        Expect(reg.Tests()[0].metadata.HasTag("ABCDEFGHIJKLMNOPQRSTUVWXYZ")).ToBeTrue();
    }

    void TestTagOfAllLowercase() {
        Registry reg;
        reg.Add<TagValDummy>("S", "T", &TagValDummy::Noop, {"abcdefghijklmnopqrstuvwxyz"});
        Expect(reg.Tests()[0].metadata.HasTag("abcdefghijklmnopqrstuvwxyz")).ToBeTrue();
    }

    // ========================================================================
    // ASCII boundary characters adjacent to the valid ranges
    // ========================================================================

    void TestCharSlashBeforeDigit0() {
        // '/' is ASCII 47, just before '0' (ASCII 48)
        Expect(DiesOnTerminate([] {
            Registry reg;
            reg.Add<TagValDummy>("S", "T", &TagValDummy::Noop, {"/"});
        })).ToBeTrue();
    }

    void TestCharColonAfterDigit9() {
        // ':' is ASCII 58, just after '9' (ASCII 57)
        Expect(DiesOnTerminate([] {
            Registry reg;
            reg.Add<TagValDummy>("S", "T", &TagValDummy::Noop, {":"});
        })).ToBeTrue();
    }

    void TestCharAtBeforeUpperA() {
        // '@' is ASCII 64, just before 'A' (ASCII 65)
        Expect(DiesOnTerminate([] {
            Registry reg;
            reg.Add<TagValDummy>("S", "T", &TagValDummy::Noop, {"@"});
        })).ToBeTrue();
    }

    void TestCharOpenBracketAfterUpperZ() {
        // '[' is ASCII 91, just after 'Z' (ASCII 90)
        Expect(DiesOnTerminate([] {
            Registry reg;
            reg.Add<TagValDummy>("S", "T", &TagValDummy::Noop, {"["});
        })).ToBeTrue();
    }

    void TestCharBacktickBeforeLowerA() {
        // '`' is ASCII 96, just before 'a' (ASCII 97)
        Expect(DiesOnTerminate([] {
            Registry reg;
            reg.Add<TagValDummy>("S", "T", &TagValDummy::Noop, {"`"});
        })).ToBeTrue();
    }

    void TestCharOpenBraceAfterLowerZ() {
        // '{' is ASCII 123, just after 'z' (ASCII 122)
        Expect(DiesOnTerminate([] {
            Registry reg;
            reg.Add<TagValDummy>("S", "T", &TagValDummy::Noop, {"{"});
        })).ToBeTrue();
    }

    // ========================================================================
    // AddTests with invalid per-test tag aborts
    // ========================================================================

    void TestAddTestsWithInvalidGroupTag() {
        Expect(DiesOnTerminate([] {
            Registry reg;
            TagValDummy::AddTests(
                reg, "S",
                {{.name = "Noop", .method = &TagValDummy::Noop, .tags = {"valid", "inv@lid"}}});
        })).ToBeTrue();
    }

    static void Register(Registry& r) {
        AddTests(
            r, "TagValidationAdversarialSuite",
            {
                // Single-char valid
                {.name = "TestSingleCharLowercaseA",
                 .method = &TagValidationAdversarialSuite::TestSingleCharLowercaseA},
                {.name = "TestSingleCharLowercaseZ",
                 .method = &TagValidationAdversarialSuite::TestSingleCharLowercaseZ},
                {.name = "TestSingleCharUppercaseA",
                 .method = &TagValidationAdversarialSuite::TestSingleCharUppercaseA},
                {.name = "TestSingleCharUppercaseZ",
                 .method = &TagValidationAdversarialSuite::TestSingleCharUppercaseZ},
                {.name = "TestSingleCharDigit0",
                 .method = &TagValidationAdversarialSuite::TestSingleCharDigit0},
                {.name = "TestSingleCharDigit9",
                 .method = &TagValidationAdversarialSuite::TestSingleCharDigit9},
                {.name = "TestSingleCharUnderscore",
                 .method = &TagValidationAdversarialSuite::TestSingleCharUnderscore},
                {.name = "TestSingleCharHyphen",
                 .method = &TagValidationAdversarialSuite::TestSingleCharHyphen},
                // All-hyphens, all-underscores
                {.name = "TestAllHyphens",
                 .method = &TagValidationAdversarialSuite::TestAllHyphens},
                {.name = "TestAllUnderscores",
                 .method = &TagValidationAdversarialSuite::TestAllUnderscores},
                {.name = "TestMixedHyphensUnderscores",
                 .method = &TagValidationAdversarialSuite::TestMixedHyphensUnderscores},
                // Single-char invalid
                {.name = "TestSingleCharSpace",
                 .method = &TagValidationAdversarialSuite::TestSingleCharSpace},
                {.name = "TestSingleCharAt",
                 .method = &TagValidationAdversarialSuite::TestSingleCharAt},
                {.name = "TestSingleCharOpenBracket",
                 .method = &TagValidationAdversarialSuite::TestSingleCharOpenBracket},
                {.name = "TestSingleCharCloseBracket",
                 .method = &TagValidationAdversarialSuite::TestSingleCharCloseBracket},
                {.name = "TestSingleCharSemicolon",
                 .method = &TagValidationAdversarialSuite::TestSingleCharSemicolon},
                {.name = "TestSingleCharSlash",
                 .method = &TagValidationAdversarialSuite::TestSingleCharSlash},
                {.name = "TestSingleCharDot",
                 .method = &TagValidationAdversarialSuite::TestSingleCharDot},
                {.name = "TestSingleCharColon",
                 .method = &TagValidationAdversarialSuite::TestSingleCharColon},
                {.name = "TestSingleCharComma",
                 .method = &TagValidationAdversarialSuite::TestSingleCharComma},
                // Whitespace variants
                {.name = "TestTagWithNewline",
                 .method = &TagValidationAdversarialSuite::TestTagWithNewline},
                {.name = "TestTagWithTab",
                 .method = &TagValidationAdversarialSuite::TestTagWithTab},
                {.name = "TestTagWithCarriageReturn",
                 .method = &TagValidationAdversarialSuite::TestTagWithCarriageReturn},
                {.name = "TestTagWithEmbeddedSpace",
                 .method = &TagValidationAdversarialSuite::TestTagWithEmbeddedSpace},
                // Null bytes and non-ASCII
                {.name = "TestTagWithEmbeddedNull",
                 .method = &TagValidationAdversarialSuite::TestTagWithEmbeddedNull},
                {.name = "TestTagWithHighByte0x80",
                 .method = &TagValidationAdversarialSuite::TestTagWithHighByte0x80},
                {.name = "TestTagWithHighByteFF",
                 .method = &TagValidationAdversarialSuite::TestTagWithHighByteFF},
                {.name = "TestTagWithUtf8Sequence",
                 .method = &TagValidationAdversarialSuite::TestTagWithUtf8Sequence},
                // Long tags
                {.name = "TestVeryLongValidTag",
                 .method = &TagValidationAdversarialSuite::TestVeryLongValidTag},
                {.name = "TestVeryLongInvalidTag",
                 .method = &TagValidationAdversarialSuite::TestVeryLongInvalidTag},
                // Mixed valid/invalid in same call
                {.name = "TestValidThenInvalidAborts",
                 .method = &TagValidationAdversarialSuite::TestValidThenInvalidAborts},
                {.name = "TestInvalidThenValidAborts",
                 .method = &TagValidationAdversarialSuite::TestInvalidThenValidAborts},
                {.name = "TestMultipleInvalidTagsAborts",
                 .method = &TagValidationAdversarialSuite::TestMultipleInvalidTagsAborts},
                // Dedup before validation
                {.name = "TestDuplicateInvalidTagDedupedBeforeValidation",
                 .method = &TagValidationAdversarialSuite::
                               TestDuplicateInvalidTagDedupedBeforeValidation},
                // Diagnostic content
                {.name = "TestDiagnosticContainsErrorKeyword",
                 .method = &TagValidationAdversarialSuite::TestDiagnosticContainsErrorKeyword},
                {.name = "TestDiagnosticContainsFlulTestPrefix",
                 .method = &TagValidationAdversarialSuite::TestDiagnosticContainsFlulTestPrefix},
                {.name = "TestDiagnosticContainsOffendingTag",
                 .method = &TagValidationAdversarialSuite::TestDiagnosticContainsOffendingTag},
                {.name = "TestDiagnosticContainsSuiteAndTestName",
                 .method = &TagValidationAdversarialSuite::TestDiagnosticContainsSuiteAndTestName},
                {.name = "TestDiagnosticContainsAllowlistPattern",
                 .method = &TagValidationAdversarialSuite::TestDiagnosticContainsAllowlistPattern},
                // Sanity: valid tags near boundary
                {.name = "TestMixedCaseAlphanumericTag",
                 .method = &TagValidationAdversarialSuite::TestMixedCaseAlphanumericTag},
                {.name = "TestTagOfAllDigits",
                 .method = &TagValidationAdversarialSuite::TestTagOfAllDigits},
                {.name = "TestTagOfAllUppercase",
                 .method = &TagValidationAdversarialSuite::TestTagOfAllUppercase},
                {.name = "TestTagOfAllLowercase",
                 .method = &TagValidationAdversarialSuite::TestTagOfAllLowercase},
                // ASCII boundary adjacents
                {.name = "TestCharSlashBeforeDigit0",
                 .method = &TagValidationAdversarialSuite::TestCharSlashBeforeDigit0},
                {.name = "TestCharColonAfterDigit9",
                 .method = &TagValidationAdversarialSuite::TestCharColonAfterDigit9},
                {.name = "TestCharAtBeforeUpperA",
                 .method = &TagValidationAdversarialSuite::TestCharAtBeforeUpperA},
                {.name = "TestCharOpenBracketAfterUpperZ",
                 .method = &TagValidationAdversarialSuite::TestCharOpenBracketAfterUpperZ},
                {.name = "TestCharBacktickBeforeLowerA",
                 .method = &TagValidationAdversarialSuite::TestCharBacktickBeforeLowerA},
                {.name = "TestCharOpenBraceAfterLowerZ",
                 .method = &TagValidationAdversarialSuite::TestCharOpenBraceAfterLowerZ},
                // AddTests per-test invalid
                {.name = "TestAddTestsWithInvalidGroupTag",
                 .method = &TagValidationAdversarialSuite::TestAddTestsWithInvalidGroupTag},
            });
    }
};

// NOLINTEND(readability-convert-member-functions-to-static,readability-make-member-function-const)

namespace tag_validation_adversarial_test {
void Register(Registry& r) {  // NOLINT(misc-use-internal-linkage)
    TagValidationAdversarialSuite::Register(r);
}
}  // namespace tag_validation_adversarial_test
