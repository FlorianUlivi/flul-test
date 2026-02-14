# My C++ software test framework - flul-test

- This project is more about trying and learning and less about practicality.
- Target platforms: macOS, Linux (Debian Trixie reference environment)
- Base is C++23, CMake 3.31+
   - Reference toolchain: CMake 3.31, Ninja, Clang/LLVM 19 with libc++
   - Minimum tested on Linux: Debian Trixie with system-default `clang++`

## Tooling (Debian Trixie)

Ensure `clang++` resolves to Clang 19 with libc++ (e.g., via
`update-alternatives`) so `std::print`/`std::println` are available. The
coverage workflow depends on the matching `llvm-profdata` and `llvm-cov`
executables from the active LLVM toolchain.

## Goal

- Create a test framework, that allows to create and execute unit tests
- A test binary should execute all tests and output test results:
  - On success: Very brief
  - On failure: What went wrong
- Use the latest and greatest C++23 features to achieve the goal
- Avoid macro usage

## Scope

### Features

1. **Test Registration** `[DONE]` - Register test functions via injected registry
2. **Test Suites** `[DONE]` - Group related tests under a named suite
   - A suite is a class that contains test methods
   - Each test runs on a fresh suite instance (no shared state between tests)
   - Suites provide the natural boundary for fixtures
3. **Test Runner** `[DONE]` - Execute all tests, track pass/fail
4. **Assertions** `[DONE]` - Expect:
   - equal
   - true
   - false
   - greater than
   - less than
   - unequal
   - throw (specific exception type)
   - no throw
   - near (floating-point near-equality): `[TODO]`
     - The framework shall provide `ToBeNear(expected, margin)` on `Expect<T>` where `T` models `std::floating_point`
     - The assertion shall pass when `std::abs(actual - expected) <= margin`
     - `margin` shall be the same type as `T` and shall be required to be non-negative; a negative margin shall trigger an `AssertionError` at runtime
     - On failure, the diagnostic output shall include the actual value, the expected value, the margin, and the computed absolute difference
   - Assertion failure throws `AssertionError`, stops the current test
   - Multi-constraint chaining: `Expect(x).ToBeGreaterThan(0).ToBeLessThan(100)`
   - Source location captured via `std::source_location` default parameter on
     `Expect` constructor — no macros needed
   - Custom types supported: comparison methods require the relevant concept
     (`std::equality_comparable`, `std::totally_ordered`); failure output uses
     `std::formatter<T>`, then `operator<<`, then `"<non-printable>"`
5. **Output** `[DONE]`:
   - On success: Test name + "PASS"
   - On failure: Test name + "FAIL" + source location + what went wrong (actual vs expected)
   - Per-test timing via `std::chrono`
6. **CTest Integration** `[DONE]`:
   - Exit code 0 when all tests pass, non-zero on any failure
   - `--list` flag to output test names (one per line) for CTest discovery
   - `--filter <pattern>` to run a subset of tests by name
   - CMake function to register the test binary with CTest
7. **Fixtures** `[DONE]` - Setup and teardown
   - Defined as overridable methods on the suite class
   - `SetUp()` runs before each test, `TearDown()` runs after
   - Fresh instance per test guarantees clean state
8. **Test Filtering** `[DONE]` - Run subset of tests by name or pattern via CLI
9. **Tags** `[TODO]` - Annotate individual tests with string labels for cross-suite selection
   - Tags are assigned at registration time as an optional argument to `Register()`
   - Multiple tags per test are supported
   - `--tag <tag>` CLI flag runs only tests carrying that tag (repeatable; multiple flags are OR-combined)
   - `--exclude-tag <tag>` CLI flag skips tests carrying that tag (repeatable)
   - Both flags compose with `--filter`: name filter applies first, tag filter second
   - `--list` output includes tags alongside test names
   - Tags have no CTest or IDE integration — they are a CLI-only convenience
10. **Randomized Execution Order** `[TODO]` - Shuffle test execution order to catch ordering dependencies
   - Default is fixed (deterministic) order; opt-in via `--randomize` CLI flag
   - Randomization applies at two levels: suite order, and test order within each suite
   - The RNG seed is printed at the start of the run so the order is reproducible
   - `--seed <N>` CLI flag accepts an integer seed to replay a specific order
   - `--randomize` with `--seed <N>` uses the provided seed instead of a random one
   - Compose with `--filter` and `--tag`: filtering happens first, then randomization is applied to the surviving tests
11. **Coverage** `[DONE]` - Generate LLVM source-based line and branch coverage reports
   via `./scripts/coverage.sh`. HTML output in `build/coverage/coverage-report/`.

### Design Decisions

- Dependency injection and no singleton class for registry
- Method chaining assertion style: `Expect(a).ToEqual(b)`
- Fail-stop: assertion failure throws `AssertionError`, runner catches per test
- CRTP base class for suite registration (register methods explicitly, no macros)
  - Registration is a static, type-level operation — CRTP provides the derived
    type naturally for fresh instance construction per test
  - `Register()` accepts an optional `std::initializer_list<std::string_view>` tags
    parameter; omitting it is zero-overhead (no tags stored)
- Isolation model: one fresh suite object per test method, no state leaks
- Assertion methods return `Expect<T>&` to allow chaining multiple constraints
- `std::source_location` default parameter captures call-site file/line/function
  at zero cost — no macros, no stack traces

### C++23 Features to Exploit


| Feature | Application |
|---|---|
| `std::print` / `std::format` | All output |
| Deducing `this` | Assertion method chaining, builder patterns |
| `constexpr` expansion | Compile-time assertion validation where possible |
| `std::source_location` | Assertion failure diagnostics (file, line, function) |

