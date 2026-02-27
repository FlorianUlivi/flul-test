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

#### Test Structure

1. **Test Registration** `[DONE]` `#REG` - Register test functions via injected registry
2. **Test Suites** `[DONE]` `#SUITE` - Group related tests under a named suite
   - A suite is a class that contains test methods
   - Each test runs on a fresh suite instance (no shared state between tests)
   - Suites provide the natural boundary for fixtures
3. **Fixtures** `[DONE]` `#FIX` - Setup and teardown
   - Defined as overridable methods on the suite class
   - `SetUp()` runs before each test, `TearDown()` runs after
   - Fresh instance per test guarantees clean state

#### Test Execution Control

1. **Test Runner** `[DONE]` `#RUN` - Execute all tests, track pass/fail
2. **Test Filtering** `[DONE]` `#FILT` - Run subset of tests by name or pattern via CLI
3. **Randomized Execution Order** `[TODO]` `#RAND` - Shuffle test execution order to catch ordering dependencies
   - Default is fixed (deterministic) order; opt-in via `--randomize` CLI flag
   - Randomization applies at two levels: suite order, and test order within each suite
   - The RNG seed is printed at the start of the run, before any test output, in the format `[seed: N]`, so the order is reproducible
   - `--seed <N>` CLI flag accepts a non-negative 32-bit integer seed (`uint32_t` range: 0–4294967295) to replay a specific order
   - `--randomize` with `--seed <N>` uses the provided seed instead of a random one
   - `--seed <N>` without `--randomize` implicitly enables randomization as if `--randomize` had been passed
   - An out-of-range or non-integer value passed to `--seed` shall cause the runner to exit with a non-zero exit code and a descriptive error message
   - Compose with `--filter` and `--tag`: filtering happens first, then randomization is applied to the surviving tests
   - `--list` and `--list-verbose` output is unaffected by `--randomize` or `--seed`; listing always uses the fixed registration order
   - Tests are executed and reported in the randomized order (output order matches execution order)

#### Per-Test Metadata

1. **Tags** `[TODO]` `#TAG` - Annotate individual tests with string labels for cross-suite selection
   - Tags are assigned at registration time as an optional argument to `Register()`
   - Multiple tags per test are supported
   - `--tag <tag>` CLI flag runs only tests carrying that tag (repeatable; multiple flags are OR-combined)
   - `--exclude-tag <tag>` CLI flag skips tests carrying that tag (repeatable)
   - When a test matches both `--tag` and `--exclude-tag`, `--exclude-tag` takes precedence and the test is excluded
   - Both flags compose with `--filter`: name filter applies first, tag filter second
   - `--list` output is unchanged and contains no tags, preserving CTest discovery compatibility
   - `--list-verbose` flag outputs one test per line with tags appended, e.g. `SuiteName::TestName [tag1, tag2]`; intended for human inspection only and has no CTest integration
   - Tags have no CTest or IDE integration — they are a CLI-only convenience
2. **Expected Failure (xfail)** `[TODO]` `#XFAIL` - Mark individual tests as expected to fail
   - Xfail is per-test metadata set via the fluent `Test<Derived>` builder's `.ExpectFail()` method (see `doc/xfail-design.md`)
   - A test marked xfail that **fails** is reported as **XFAIL** (expected failure) and counts as a pass for the overall exit code
   - A test marked xfail that **passes** is reported as **XPASS** (unexpected pass) and counts as a failure for the overall exit code
   - Xfail interacts with tags and filtering: it is orthogonal metadata, applied after filtering just like any other per-test attribute
   - The `--list` output is unchanged (no marker added), preserving CTest discovery compatibility
   - The `--list-verbose` output includes the `[xfail]` marker
   - Xfail is set at registration time and cannot be toggled via CLI flags
   - The summary line includes xfail/xpass counts alongside pass/fail
3. **Skip** `[TODO]` `#SKIP` - Mark individual tests to be excluded from execution
   - Skip is per-test metadata set via the fluent `Test<Derived>` builder's `.Skip()` method (analogous to `.ExpectFail()`)
   - A skipped test is not executed
   - A skipped test is reported as **SKIP** and counts as neither pass nor fail for the overall exit code
   - Skip is orthogonal to tags and filtering: it is applied after filtering, so a filtered-out test is never reported as skipped
   - The `--list` output is unchanged (no marker added), preserving CTest discovery compatibility
   - The `--list-verbose` output includes the `[skip]` marker
   - Skip is set at registration time and cannot be toggled via CLI flags
   - The summary line includes the skip count alongside pass/fail/xfail/xpass counts
4. **Timeout** `[TODO]` `#TMO` - Flag tests that exceed a per-test time limit
   - Timeout is per-test metadata set via the fluent `Test<Derived>` builder's `.Timeout(duration)` method, where `duration` is a `std::chrono::duration`
   - After a test completes, the runner compares elapsed time against the timeout; if exceeded, the outcome is overridden to **TIMEOUT**
   - TIMEOUT counts as a failure for the overall exit code
   - The framework does **not** interrupt or abort a running test — a hung test still blocks. Timeout only detects tests that ran too long after they finish.
   - Timeout is orthogonal to tags and filtering: applied after filtering, so a filtered-out test is never subject to its timeout
   - The `--list` output is unchanged (no marker added), preserving CTest discovery compatibility
   - The `--list-verbose` output includes the `[timeout: Xms]` marker
   - Timeout is set at registration time and cannot be toggled via CLI flags
   - The summary line includes the timeout count alongside pass/fail/skip/xfail/xpass counts

#### Assertions

1. **Assertions** `[DONE]` `#ASSRT` - Expect:
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

#### Reporting & Output

1. **Output** `[DONE]` `#OUT`:
   - On success: Test name + "PASS"
   - On failure: Test name + "FAIL" + source location + what went wrong (actual vs expected)
   - Per-test timing via `std::chrono`

#### Tooling & Integration

1. **CTest Integration** `[DONE]` `#CTEST`:
   - Exit code 0 when all tests pass, non-zero on any failure
   - `--list` flag to output test names (one per line) for CTest discovery
   - `--filter <pattern>` to run a subset of tests by name
   - CMake function to register the test binary with CTest
2. **Coverage** `[DONE]` `#COV` - Generate LLVM source-based line and branch coverage reports
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
- `TestMetadata` value type holds test identity and configuration (names, tags,
  flags such as xfail or timeout); owned by `TestEntry`, borrowed
  by `TestResult` via `std::reference_wrapper<const TestMetadata>`
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

