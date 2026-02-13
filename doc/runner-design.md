# Runner & CTest Integration — Detailed Design

## 1. Overview

This document covers the test execution and CTest integration layer: `TestResult`,
`Runner`, `Run()`, and the CMake discovery mechanism. These components execute
registered tests, capture timing and failure diagnostics, output results, and
integrate with CTest for per-test discovery.

### File Map

| File | Purpose |
|---|---|
| `include/flul/test/test_result.hpp` | `TestResult` value type |
| `include/flul/test/runner.hpp` | `Runner` class — iterates tests, captures results, outputs |
| `include/flul/test/run.hpp` | `Run()` free function — CLI parsing + Runner wiring |
| `cmake/FlulTest.cmake` | `flul_test_discover()` CMake function |
| `cmake/FlulTestDiscovery.cmake` | Post-build script for per-test CTest discovery |

All C++ files are header-only (`inline`), consistent with the existing `INTERFACE`
library approach.

### Dependency Graph

```
run.hpp ────────────► runner.hpp ──────► registry.hpp
    │                     │                  │
    │                     ├──────────► test_result.hpp ──► <chrono>
    │                     │                                 <optional>
    │                     │                                 <string_view>
    │                     │
    │                     └──────────► assertion_error.hpp
    │
    └───────────────► registry.hpp
                           │
                           └──► (existing dependency chain)
```

## 2. `TestResult`

### Interface

```cpp
namespace flul::test {

struct TestResult {
    std::string_view suite_name;
    std::string_view test_name;
    bool passed;
    std::chrono::nanoseconds duration;
    std::optional<AssertionError> error;
};

}  // namespace flul::test
```

### Design Notes

- `string_view` is safe — views point into `TestEntry` data which uses string
  literals (static storage duration).
- `std::chrono::nanoseconds` is the duration type. The output layer converts
  to human-readable units at print time.
- `std::optional<AssertionError>` is empty on pass, populated on failure.
  This carries the full assertion context (actual, expected, source location)
  for the output layer to format.
- Plain aggregate — no constructor, no methods. Produced by `Runner`,
  consumed by output formatting.

## 3. `Runner`

### Interface

```cpp
namespace flul::test {

class Runner {
public:
    explicit Runner(const Registry& registry);

    auto RunAll() -> int;

private:
    const Registry& registry_;

    static auto RunTest(const TestEntry& entry) -> TestResult;
    static void PrintResult(const TestResult& result);
    static void PrintSummary(std::span<const TestResult> results);
    static auto FormatDuration(std::chrono::nanoseconds duration) -> std::string;
};

}  // namespace flul::test
```

### Constructor

```cpp
inline Runner::Runner(const Registry& registry)
    : registry_(registry) {}
```

Stores a const reference. The `Registry` must outlive the `Runner`. This is
naturally satisfied since both live in `main()`.

### `RunAll`

```cpp
inline auto Runner::RunAll() -> int {
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
```

**Flow:**
1. Get test span from registry
2. Run each test, print its result immediately, accumulate results
3. Print summary
4. Return 0 if all passed, 1 if any failed

### `RunTest`

```cpp
inline auto Runner::RunTest(const TestEntry& entry) -> TestResult {
    auto start = std::chrono::steady_clock::now();

    try {
        entry.callable();
        auto duration = std::chrono::steady_clock::now() - start;
        return {entry.suite_name, entry.test_name, true, duration, std::nullopt};
    } catch (const AssertionError& e) {
        auto duration = std::chrono::steady_clock::now() - start;
        return {entry.suite_name, entry.test_name, false, duration, e};
    } catch (const std::exception& e) {
        auto duration = std::chrono::steady_clock::now() - start;
        auto loc = std::source_location::current();
        return {entry.suite_name, entry.test_name, false, duration,
                AssertionError("threw: " + std::string(e.what()), "no exception", loc)};
    } catch (...) {
        auto duration = std::chrono::steady_clock::now() - start;
        auto loc = std::source_location::current();
        return {entry.suite_name, entry.test_name, false, duration,
                AssertionError("unknown exception", "no exception", loc)};
    }
}
```

**Exception handling:**

| Caught | `passed` | `error.actual` | `error.expected` |
|---|---|---|---|
| (none) | `true` | — | — |
| `AssertionError& e` | `false` | from `e` | from `e` |
| `std::exception& e` | `false` | `"threw: " + e.what()` | `"no exception"` |
| `...` | `false` | `"unknown exception"` | `"no exception"` |

**Timing:** `steady_clock` is monotonic and high-resolution on macOS. The
duration includes `SetUp`, test body, and `TearDown` (all wrapped inside
`callable()`).

**Static method:** `RunTest` has no dependency on `Runner` instance state —
it's a pure function of a `TestEntry`. Marked `static` to make this explicit.

### `PrintResult`

```cpp
inline void Runner::PrintResult(const TestResult& result) {
    auto tag = result.passed ? "PASS" : "FAIL";
    std::println("[ {} ] {}::{} ({})", tag, result.suite_name, result.test_name,
                 FormatDuration(result.duration));

    if (!result.passed && result.error) {
        std::println("  {}", result.error->what());
    }
}
```

**Output per test:**

Pass:
```
[ PASS ] MathSuite::TestAddition (0.12ms)
```

Fail:
```
[ FAIL ] MathSuite::TestDivision (0.05ms)
  /path/file.cpp:42: assertion failed
    expected: 0
      actual: 1
```

The `what()` string from `AssertionError` already contains file, line,
expected, and actual — no additional formatting needed. The two-space indent
visually groups the error with its test.

**Plain text only** — no ANSI color codes.

### `PrintSummary`

```cpp
inline void Runner::PrintSummary(std::span<const TestResult> results) {
    auto passed = std::ranges::count_if(results, &TestResult::passed);
    auto failed = static_cast<std::ptrdiff_t>(results.size()) - passed;

    std::println("");
    std::println("{} tests, {} passed, {} failed", results.size(), passed, failed);
}
```

**Output:**
```

2 tests, 1 passed, 1 failed
```

Blank line separates summary from per-test output. When all tests pass,
the summary is the only line that mentions a count — the per-test output
is already minimal.

### `FormatDuration`

```cpp
inline auto Runner::FormatDuration(std::chrono::nanoseconds ns) -> std::string {
    if (ns < 1us) return std::format("{}ns", ns.count());
    if (ns < 1ms) return std::format("{:.2f}µs", ns.count() / 1'000.0);
    if (ns < 1s)  return std::format("{:.2f}ms", ns.count() / 1'000'000.0);
    return std::format("{:.2f}s", ns.count() / 1'000'000'000.0);
}
```

| Duration Range | Format | Example |
|---|---|---|
| < 1µs | `{n}ns` | `423ns` |
| < 1ms | `{n.nn}µs` | `12.34µs` |
| < 1s | `{n.nn}ms` | `0.12ms` |
| >= 1s | `{n.nn}s` | `1.05s` |

Auto-scaling picks the most readable unit. Two decimal places balance
precision and readability.

## 4. `Run()` Free Function

### Interface

```cpp
namespace flul::test {

auto Run(int argc, char* argv[], Registry& registry) -> int;

}  // namespace flul::test
```

### Implementation

```cpp
inline auto Run(int argc, char* argv[], Registry& registry) -> int {
    for (int i = 1; i < argc; ++i) {
        auto arg = std::string_view(argv[i]);

        if (arg == "--list") {
            registry.List();
            return 0;
        }
        if (arg == "--filter") {
            if (i + 1 >= argc) {
                std::println(stderr, "error: --filter requires an argument");
                return 1;
            }
            registry.Filter(argv[++i]);
        } else if (arg == "--help") {
            std::println("usage: {} [--list] [--filter <pattern>] [--help]", argv[0]);
            return 0;
        } else {
            std::println(stderr, "error: unknown option '{}'", arg);
            std::println(stderr, "usage: {} [--list] [--filter <pattern>] [--help]", argv[0]);
            return 1;
        }
    }

    Runner runner(registry);
    return runner.RunAll();
}
```

### CLI Flags

| Flag | Action | Exit Code |
|---|---|---|
| (none) | Run all tests | 0 all pass, 1 any fail |
| `--list` | Print test names, one per line | 0 |
| `--filter <pattern>` | Filter tests by substring, then run | 0/1 |
| `--help` | Print usage | 0 |
| unknown | Print error + usage | 1 |

**`--list` output** comes from `Registry::List()`, which prints
`SuiteName::TestName` per line. This is the contract for CTest discovery.

**`--filter`** delegates to `Registry::Filter()` which uses substring match.
Applied before running — compatible with `--list` if both are given (but
`--list` exits immediately, so order matters: `--list` takes precedence).

**No library dependencies** — CLI parsing is manual. The flag set is small and
fixed; a library would be overkill.

### User-Facing `main()` Pattern

```cpp
#include <flul/test/registry.hpp>
#include <flul/test/run.hpp>

#include "math_suite.hpp"
#include "string_suite.hpp"

auto main(int argc, char* argv[]) -> int {
    flul::test::Registry registry;
    MathSuite::Register(registry);
    StringSuite::Register(registry);
    return flul::test::Run(argc, argv, registry);
}
```

Three lines of user code: create registry, register suites, run. The `Run()`
function handles everything else.

## 5. CTest Integration

### Overview

CTest integration provides per-test discovery: each test method becomes an
individual CTest test entry. This gives fine-grained pass/fail reporting in
IDEs (CLion, VS Code) and CI dashboards.

The mechanism uses two CMake files:

1. **`cmake/FlulTest.cmake`** — provides `flul_test_discover()` function
2. **`cmake/FlulTestDiscovery.cmake`** — post-build discovery script

### `cmake/FlulTest.cmake`

```cmake
function(flul_test_discover TARGET)
    set(ctest_file "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}_tests.cmake")

    add_custom_command(
        TARGET ${TARGET} POST_BUILD
        BYPRODUCTS "${ctest_file}"
        COMMAND "${CMAKE_COMMAND}"
            -D "TEST_EXECUTABLE=$<TARGET_FILE:${TARGET}>"
            -D "CTEST_FILE=${ctest_file}"
            -P "${PROJECT_SOURCE_DIR}/cmake/FlulTestDiscovery.cmake"
        VERBATIM
    )

    set_property(DIRECTORY
        APPEND PROPERTY TEST_INCLUDE_FILES "${ctest_file}"
    )
endfunction()
```

**How it works:**
1. `add_custom_command` with `POST_BUILD` runs after the test binary is built
2. It invokes `FlulTestDiscovery.cmake` as a CMake script (`-P`)
3. The script generates a `.cmake` file containing `add_test()` calls
4. `TEST_INCLUDE_FILES` directory property tells CTest to include this file

**`BYPRODUCTS`** declares the generated `.cmake` file so Ninja knows about it.

**`VERBATIM`** ensures proper argument quoting across platforms.

### `cmake/FlulTestDiscovery.cmake`

```cmake
execute_process(
    COMMAND "${TEST_EXECUTABLE}" --list
    OUTPUT_VARIABLE output
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE result
)

if(NOT result EQUAL 0)
    message(FATAL_ERROR "Test discovery failed for '${TEST_EXECUTABLE}' (exit code ${result})")
endif()

if(output STREQUAL "")
    file(WRITE "${CTEST_FILE}" "")
    return()
endif()

string(REPLACE "\n" ";" test_list "${output}")

file(WRITE "${CTEST_FILE}" "")
foreach(test IN LISTS test_list)
    if(NOT test STREQUAL "")
        file(APPEND "${CTEST_FILE}"
            "add_test(\"${test}\" \"${TEST_EXECUTABLE}\" \"--filter\" \"${test}\")\n"
        )
    endif()
endforeach()
```

**Discovery protocol:**
1. Runs `test_binary --list` → one `SuiteName::TestName` per line
2. Parses output into a CMake list (split on newlines)
3. For each test name: `add_test("SuiteName::TestName" binary --filter "SuiteName::TestName")`
4. Empty output (no tests) → empty file (no CTest tests registered)

**Error handling:** Non-zero exit from `--list` is `FATAL_ERROR` — this
means the binary failed to run, which should be visible immediately.

### CMake Usage

```cmake
include(cmake/FlulTest.cmake)

add_executable(my_tests test/main.cpp)
target_link_libraries(my_tests PRIVATE flul-test)
set_project_warnings(my_tests)
flul_test_discover(my_tests)
```

After build, `ctest` sees individual tests:
```
$ ctest --test-dir build/debug -N
Test #1: MathSuite::TestAddition
Test #2: MathSuite::TestSubtraction
Test #3: StringSuite::TestConcat
```

## 6. Design Rationale Summary

| Decision | Choice | Rationale |
|---|---|---|
| Header-only Runner | `inline` in `.hpp` | Consistent with project; Runner is small and non-template only in `RunAll` |
| `Run()` as free function | Separate header from `Runner` | CLI parsing is orthogonal to test execution |
| `steady_clock` for timing | Monotonic, high resolution | Standard practice; not affected by wall-clock adjustments |
| Catch all exception types | `AssertionError`, `std::exception`, `...` | Runner never crashes; unexpected exceptions become test failures |
| Per-test CTest discovery | Post-build `--list` parsing | Individual test visibility in IDEs and CI dashboards |
| Duration auto-scaling | ns/µs/ms/s with 2 decimal places | Human-readable without clutter |
| Plain text output | No ANSI colors | Clean in all contexts (pipes, CI logs, redirected output) |
| `string_view` in `TestResult` | Views into `TestEntry` data | Zero-copy; safe because source is string literals |
| `optional<AssertionError>` | Empty on pass | Avoids separate error channel; natural "no error" representation |
| Manual CLI parsing | No library | Three flags; a library adds complexity with no benefit |

## 7. Usage Examples

### Minimal Test Binary

```cpp
#include <flul/test/expect.hpp>
#include <flul/test/registry.hpp>
#include <flul/test/run.hpp>

using flul::test::Expect;
using flul::test::Registry;
using flul::test::Suite;

class MathSuite : public Suite<MathSuite> {
public:
    void TestAddition() {
        Expect(1 + 1).ToEqual(2);
    }

    void TestNegation() {
        Expect(-1).ToBeLessThan(0);
    }

    static void Register(Registry& r) {
        AddTests(r, "MathSuite", {
            {"TestAddition", &MathSuite::TestAddition},
            {"TestNegation", &MathSuite::TestNegation},
        });
    }
};

auto main(int argc, char* argv[]) -> int {
    Registry registry;
    MathSuite::Register(registry);
    return flul::test::Run(argc, argv, registry);
}
```

### Output — All Pass

```
[ PASS ] MathSuite::TestAddition (0.02ms)
[ PASS ] MathSuite::TestNegation (0.01ms)

2 tests, 2 passed, 0 failed
```

### Output — With Failure

```
[ PASS ] MathSuite::TestAddition (0.02ms)
[ FAIL ] MathSuite::TestNegation (0.01ms)
  /path/to/test.cpp:15: assertion failed
    expected: less than 0
      actual: 1

2 tests, 1 passed, 1 failed
```

### Suite with Fixtures

```cpp
class DbSuite : public Suite<DbSuite> {
    int connection_id_ = -1;

public:
    void SetUp() override { connection_id_ = OpenTestDb(); }
    void TearDown() override { CloseTestDb(connection_id_); }

    void TestConnection() {
        Expect(connection_id_).ToBeGreaterThan(0);
    }

    void TestQuery() {
        auto result = Query(connection_id_, "SELECT 1");
        Expect(result).ToEqual(1);
    }

    static void Register(Registry& r) {
        AddTests(r, "DbSuite", {
            {"TestConnection", &DbSuite::TestConnection},
            {"TestQuery", &DbSuite::TestQuery},
        });
    }
};
```

### CTest Integration

```cmake
cmake_minimum_required(VERSION 4.0)
project(my_project LANGUAGES CXX)

find_package(flul-test REQUIRED)  # or add_subdirectory
include(cmake/FlulTest.cmake)

add_executable(my_tests test/main.cpp)
target_link_libraries(my_tests PRIVATE flul-test)
set_project_warnings(my_tests)
flul_test_discover(my_tests)
```

```
$ cmake --build --preset debug
$ ctest --preset debug
Test project /path/to/build/debug
    Start 1: MathSuite::TestAddition
1/3 Test #1: MathSuite::TestAddition ......   Passed    0.01 sec
    Start 2: MathSuite::TestNegation
2/3 Test #2: MathSuite::TestNegation ......   Passed    0.01 sec
    Start 3: DbSuite::TestConnection
3/3 Test #3: DbSuite::TestConnection ......   Passed    0.02 sec

100% tests passed, 0 tests failed out of 3
```
