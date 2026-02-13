#ifndef FLUL_TEST_RUNNER_HPP_
#define FLUL_TEST_RUNNER_HPP_

#include <chrono>
#include <exception>
#include <format>
#include <print>
#include <ranges>
#include <source_location>
#include <span>
#include <string>
#include <vector>

#include "flul/test/assertion_error.hpp"
#include "flul/test/registry.hpp"
#include "flul/test/test_result.hpp"

namespace flul::test {

class Runner {
   public:
    explicit Runner(const Registry& registry)
        : registry_(registry) {}  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)

    auto RunAll() -> int {
        auto tests = registry_.Tests();
        std::vector<TestResult> results;
        results.reserve(tests.size());

        for (const auto& entry : tests) {
            auto result = RunTest(entry);
            PrintResult(result);
            results.push_back(std::move(result));
        }

        PrintSummary(results);

        return std::ranges::all_of(results, &TestResult::passed) ? 0 : 1;
    }

   private:
    // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members): Registry owned by caller (main)
    const Registry& registry_;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)

    static auto RunTest(const TestEntry& entry) -> TestResult {
        using namespace std::chrono;  // NOLINT(google-build-using-namespace)

        auto start = steady_clock::now();

        try {
            entry.callable();
            auto duration = steady_clock::now() - start;
            return {.suite_name = entry.suite_name,
                    .test_name = entry.test_name,
                    .passed = true,
                    .duration = duration,
                    .error = std::nullopt};
        } catch (const AssertionError& e) {
            auto duration = steady_clock::now() - start;
            return {.suite_name = entry.suite_name,
                    .test_name = entry.test_name,
                    .passed = false,
                    .duration = duration,
                    .error = e};
        } catch (const std::exception& e) {
            auto duration = steady_clock::now() - start;
            auto loc = std::source_location::current();
            return {
                .suite_name = entry.suite_name,
                .test_name = entry.test_name,
                .passed = false,
                .duration = duration,
                .error = AssertionError("threw: " + std::string(e.what()), "no exception", loc)};
        } catch (...) {
            auto duration = steady_clock::now() - start;
            auto loc = std::source_location::current();
            return {.suite_name = entry.suite_name,
                    .test_name = entry.test_name,
                    .passed = false,
                    .duration = duration,
                    .error = AssertionError("unknown exception", "no exception", loc)};
        }
    }

    static void PrintResult(const TestResult& result) {
        const auto* tag = result.passed ? "PASS" : "FAIL";
        std::println("[ {} ] {}::{} ({})", tag, result.suite_name, result.test_name,
                     FormatDuration(result.duration));

        if (!result.passed && result.error) {
            std::println("  {}", result.error->what());
        }
    }

    static void PrintSummary(std::span<const TestResult> results) {
        auto passed = std::ranges::count_if(results, &TestResult::passed);
        auto failed = static_cast<std::ptrdiff_t>(results.size()) - passed;

        std::println("");
        std::println("{} tests, {} passed, {} failed", results.size(), passed, failed);
    }

    static auto FormatDuration(std::chrono::nanoseconds ns) -> std::string {
        using namespace std::chrono_literals;  // NOLINT(google-build-using-namespace)
        if (ns < 1us) {
            return std::format("{}ns", ns.count());
        }
        if (ns < 1ms) {
            return std::format("{:.2f}\u00b5s", static_cast<double>(ns.count()) / 1'000.0);
        }
        if (ns < 1s) {
            return std::format("{:.2f}ms", static_cast<double>(ns.count()) / 1'000'000.0);
        }
        return std::format("{:.2f}s", static_cast<double>(ns.count()) / 1'000'000'000.0);
    }
};

}  // namespace flul::test

#endif  // FLUL_TEST_RUNNER_HPP_
