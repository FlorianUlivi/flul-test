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
```

## 2. `TestResult`

```cpp
struct TestResult {
    std::string_view suite_name;
    std::string_view test_name;
    bool passed;
    std::chrono::nanoseconds duration;
    std::optional<AssertionError> error;
};
```

Plain aggregate — no constructor, no methods. Produced by `Runner`, consumed
by output formatting. `string_view` is safe (views into string literals).
`optional<AssertionError>` is empty on pass, populated on failure with the
full assertion context.

## 3. `Runner`

```cpp
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
```

**Constructor** — stores a const reference to `Registry`. The `Registry` must
outlive the `Runner` (naturally satisfied since both live in `main()`).

**`RunAll`** — iterates tests from `registry_.Tests()`, calls `RunTest` for
each, prints each result immediately, accumulates results, prints summary.
Returns 0 if all passed, 1 if any failed.

**`RunTest`** — calls `entry.callable()`, times it with `steady_clock`, catches
exceptions. Three catch levels: `AssertionError` (test assertion failed),
`std::exception` (unexpected), `...` (unknown). Runner never crashes; unexpected
exceptions become test failures.

**`PrintResult`** — prints `[ PASS ]` or `[ FAIL ]` with test name and duration.
On failure, prints the error's `what()` indented below. Plain text only — no
ANSI colors.

**`PrintSummary`** — prints total, passed, and failed counts.

**`FormatDuration`** — auto-scales duration to the most readable unit (ns, us,
ms, s) with two decimal places.

## 4. `Run()` Free Function

```cpp
auto Run(int argc, char* argv[], Registry& registry) -> int;
```

CLI parsing + wiring. Separate from `Runner` because CLI parsing is orthogonal
to test execution.

### CLI Flags

| Flag | Action | Exit Code |
|---|---|---|
| (none) | Run all tests | 0 all pass, 1 any fail |
| `--list` | Print test names, one per line | 0 |
| `--filter <pattern>` | Filter tests by substring, then run | 0/1 |
| `--help` | Print usage | 0 |
| unknown | Print error + usage | 1 |

`--list` output comes from `Registry::List()` — the contract for CTest
discovery. No library dependencies for CLI parsing; the flag set is small
and fixed.

### User-facing `main()` pattern

```cpp
auto main(int argc, char* argv[]) -> int {
    flul::test::Registry registry;
    MathSuite::Register(registry);
    StringSuite::Register(registry);
    return flul::test::Run(argc, argv, registry);
}
```

## 5. CTest Integration

CTest integration provides per-test discovery: each test method becomes an
individual CTest test entry for fine-grained reporting in IDEs and CI.

### Mechanism

Two CMake files:

1. **`cmake/FlulTest.cmake`** — provides `flul_test_discover(TARGET)`. Uses
   `add_custom_command(POST_BUILD)` to invoke the discovery script after the
   test binary is built. `TEST_INCLUDE_FILES` directory property tells CTest
   to include the generated file.

2. **`cmake/FlulTestDiscovery.cmake`** — post-build script. Runs
   `test_binary --list`, parses output (one name per line), generates
   `add_test()` calls with `--filter` for each test. Non-zero exit from
   `--list` is `FATAL_ERROR`.

### CMake Usage

```cmake
include(cmake/FlulTest.cmake)

add_executable(my_tests test/main.cpp)
target_link_libraries(my_tests PRIVATE flul-test)
flul_test_discover(my_tests)
```

## 6. Design Rationale Summary

| Decision | Choice | Rationale |
|---|---|---|
| Header-only Runner | `inline` in `.hpp` | Consistent with project; Runner is small |
| `Run()` as free function | Separate header from `Runner` | CLI parsing is orthogonal to test execution |
| `steady_clock` for timing | Monotonic, high resolution | Standard practice; not affected by wall-clock adjustments |
| Catch all exception types | `AssertionError`, `std::exception`, `...` | Runner never crashes; unexpected exceptions become test failures |
| Per-test CTest discovery | Post-build `--list` parsing | Individual test visibility in IDEs and CI dashboards |
| Duration auto-scaling | ns/us/ms/s with 2 decimal places | Human-readable without clutter |
| Plain text output | No ANSI colors | Clean in all contexts (pipes, CI logs, redirected output) |
| `string_view` in `TestResult` | Views into `TestEntry` data | Zero-copy; safe because source is string literals |
| `optional<AssertionError>` | Empty on pass | Avoids separate error channel; natural "no error" representation |
| Manual CLI parsing | No library | Three flags; a library adds complexity with no benefit |
