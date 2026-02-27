# Timeout — Detailed Design

## 1. Overview

Feature `#TMO` adds per-test timeout enforcement via the `Test<Derived>`
builder's `.Timeout(duration)` method. After a test completes, the runner
compares the elapsed wall-clock time against the configured timeout; if
exceeded, the outcome is overridden to `Timeout`, which counts as a failure
for the overall exit code. There is no mid-execution interruption — a hung
test still blocks forever. This design prioritizes simplicity (zero
multithreading) over the ability to kill runaway tests.

## 2. Requirements Reference

From `doc/requirements.md`, feature `#TMO`:

- Timeout set via `Test<Derived>` builder's `.Timeout(duration)` method,
  where `duration` is a `std::chrono::duration`
- After a test completes, the runner compares elapsed time against the
  timeout; if exceeded, the outcome is overridden to **TIMEOUT**
- TIMEOUT counts as a failure for the overall exit code
- The framework does **not** interrupt or abort a running test — a hung test
  still blocks; timeout only detects tests that ran too long after they finish
- Orthogonal to tags and filtering: applied after filtering, so a
  filtered-out test is never subject to its timeout
- `--list` unchanged (CTest safe, bare names only)
- `--list-verbose` includes `[timeout: Xms]` marker
- Set at registration time, not toggleable via CLI
- Summary line includes timeout count alongside pass/fail/skip/xfail/xpass

## 3. Design Goals

- Minimal delta over `#SKIP`: one new field on `TestMetadata`, one new
  `Outcome` value, one new builder method, runner-side post-test check
- Zero multithreading: no watchdog thread, no `std::jthread`, no
  `std::abort()`, no condition variables — just a clock comparison after the
  test callable returns
- Timeout overrides all other outcomes: if the time limit is exceeded, the
  result is `Timeout` regardless of whether the test passed, failed, or was
  xfail
- Preserve CTest compatibility: `--list` output is bare test names only
- Summary line always shows the timeout count (even when zero) for consistency

## 4. C++ Architecture

### 4.1 `TestMetadata` (modified: `include/flul/test/test_metadata.hpp`)

New field:

```cpp
struct TestMetadata {
    // ...existing fields (suite_name, test_name, xfail, skip, tags)...
    std::optional<std::chrono::milliseconds> timeout;
};
```

Default `std::nullopt` means no timeout. The stored type is
`std::chrono::milliseconds` — millisecond granularity is sufficient for test
timeouts and avoids template complexity from an arbitrary duration type.

### 4.2 `Outcome` (modified: `include/flul/test/outcome.hpp`)

New enumerator:

```cpp
enum class Outcome { Pass, Fail, XFail, XPass, Skip, Timeout };
```

`IsSuccess` updated: `IsSuccess(Outcome::Timeout)` returns `false`. A
timed-out test is a failure.

### 4.3 `Test<Derived>` builder (modified: `include/flul/test/test.hpp`)

New method:

```cpp
template <typename Derived>
class Test {
   public:
    // ...existing constructor, ExpectFail(), Skip()...

    template <typename Rep, typename Period>
    auto Timeout(std::chrono::duration<Rep, Period> duration) -> Test&;
};
```

`Timeout()` converts the given duration to `std::chrono::milliseconds` via
`duration_cast`, stores it in `metadata.timeout`, and returns `*this` for
chaining. Same eager-mutation pattern as `ExpectFail()` and `Skip()`.

User-facing API:

```cpp
using namespace std::chrono_literals;

static void Register(Registry& r) {
    Test<MySuite>(r, "MySuite", "TestSlow", &MySuite::TestSlow)
        .Timeout(5s);

    Test<MySuite>(r, "MySuite", "TestFast", &MySuite::TestFast)
        .Timeout(200ms);
}
```

### 4.4 Timeout enforcement in `Runner` (modified: `include/flul/test/runner.hpp`)

#### Strategy: post-test clock check

The runner measures wall-clock time around the test callable using
`std::chrono::steady_clock`. After the existing try/catch logic determines
the base outcome (Pass, Fail, XFail, XPass), if `metadata.timeout` has a
value and the elapsed time exceeds it, the outcome is overridden to
`Outcome::Timeout`.

#### `RunTest` pseudocode

```
RunTest(entry):
    if entry.metadata.skip:
        return TestResult{Skip, 0ns, nullopt}

    start = steady_clock::now()
    base_outcome = try/catch { invoke callable; determine Pass/Fail/XFail/XPass }
    elapsed = steady_clock::now() - start

    if entry.metadata.timeout has value AND elapsed > *entry.metadata.timeout:
        outcome = Outcome::Timeout
    else:
        outcome = base_outcome

    return TestResult{outcome, elapsed, error_if_any}
```

The key point: the try/catch block runs exactly as before. The timeout check
is a single `if` after the fact. No threads, no synchronization primitives.

**Limitation:** A test that hangs (infinite loop, deadlock) never returns from
the callable, so the post-test check never executes. The process blocks
forever. This is an accepted trade-off — the design catches slow tests, not
hung tests.

#### `PrintResult`

Maps `Outcome::Timeout` to label `TMO`. Unlike the old watchdog design, this
code path is reachable in normal execution — the test completed, the runner
detected the overrun, and reports it through the standard pipeline.

#### `PrintSummary`

Counts timeout alongside other outcomes. Format:

```
N tests, P passed, F failed, S skipped, X xfail, U xpass, T timed out
```

All counts shown (including zeros) for consistency.

Exit code: unchanged logic — success iff all outcomes satisfy `IsSuccess`.
Since `IsSuccess(Timeout) == false`, timed-out tests cause non-zero exit.

### 4.5 `Registry::ListVerbose` (modified: `include/flul/test/registry.hpp`)

`ListVerbose` appends `[timeout: Xms]` for tests with a timeout set.
The duration is formatted in milliseconds. Combined with other markers:

```
MySuite::TestSlow [timeout: 5000ms]
MySuite::TestBroken [xfail]
MySuite::TestSkippedSlow [skip, timeout: 200ms]
MySuite::TestTaggedSlow [fast, timeout: 1000ms]
```

`List()` is unchanged — bare test names only.

### 4.6 `Run()` (unchanged: `include/flul/test/run.hpp`)

No new CLI flags for `#TMO`. The execution order in `Run()` is unchanged from
`#SKIP`:

1. Parse all CLI args
2. Validate `--seed` if present
3. `registry.Filter(pattern)` if `--filter`
4. `registry.FilterByTag(include_tags)` if any `--tag` flags
5. `registry.ExcludeByTag(exclude_tags)` if any `--exclude-tag` flags
6. If `--list`: `registry.List()`, return 0
7. If `--list-verbose`: `registry.ListVerbose()`, return 0
8. If `--randomize` or `--seed`: print seed, `registry.Shuffle(seed)`
9. `Runner(registry).RunAll()`

### 4.7 Interaction with `#SKIP`

A test marked both skip and timeout is reported as `Skip`. The callable is
never invoked, so the timeout is irrelevant. Skip is checked before the
callable runs in `RunTest`. In `--list-verbose`, both markers appear:
`[skip, timeout: Xms]`.

### 4.8 Interaction with `#XFAIL`

Timeout overrides xfail. If a test is marked xfail and exceeds its timeout,
the outcome is `Timeout`, not `XFail` or `XPass`. The time violation is the
more important signal — the user needs to know the test is too slow, not
whether the expected failure occurred. This applies to all base outcomes:
Pass, Fail, XFail, and XPass are all overridden to Timeout if the limit is
exceeded.

### File Map

| File | Change |
|------|--------|
| `include/flul/test/test_metadata.hpp` | **Modified** — `timeout` field |
| `include/flul/test/outcome.hpp` | **Modified** — `Timeout` enumerator, `IsSuccess` updated |
| `include/flul/test/test.hpp` | **Modified** — `Timeout(duration)` builder method |
| `include/flul/test/runner.hpp` | **Modified** — post-test clock check, `PrintResult`, `PrintSummary` |
| `include/flul/test/registry.hpp` | **Modified** — `ListVerbose` includes `[timeout: Xms]` |

### Namespace

All types remain in `flul::test`. No new namespaces.

## 5. Key Design Decisions

### Post-test clock check instead of watchdog thread

The old design used `std::jthread` + `std::stop_token` +
`std::condition_variable_any` to implement a watchdog that called
`std::abort()` on deadline expiry. That design was complex (multithreading,
synchronization) and destructive (`abort()` kills the process — no summary,
no cleanup, no remaining tests).

The new design is a single `if` statement after the test returns. It catches
tests that ran too long but completed. It cannot catch tests that hang.

**Trade-off:** Hung/deadlocked tests block forever instead of being killed.
This is acceptable because (a) the common case is "test is slow but
finishes," (b) CTest has its own process-level timeout that handles true
hangs, and (c) zero multithreading eliminates an entire class of bugs.

### Timeout overrides all other outcomes

If a test exceeds its time limit, the outcome is always `Timeout`, regardless
of whether the test passed, failed, or hit the xfail path. Alternatives
considered:

- **Only override Pass/Fail, preserve XFail/XPass:** Rejected because a
  timed-out xfail test is still too slow — the time violation is orthogonal
  to correctness expectations.
- **Report both (e.g., "XFAIL + TMO"):** Rejected as unnecessary complexity
  in the outcome model. One outcome per test keeps reporting simple.

### Stored duration type is `std::chrono::milliseconds`, not a template parameter

Storing `std::optional<std::chrono::milliseconds>` rather than a generic
`duration` avoids making `TestMetadata` a template or using type erasure. The
builder method template accepts any duration type and converts via
`duration_cast`. Millisecond granularity is more than sufficient for test
timeouts.

**Trade-off:** Sub-millisecond timeouts are truncated. This is acceptable —
sub-millisecond test timeouts have no practical use.

### Elapsed time is actual wall-clock duration

The runner uses `std::chrono::steady_clock` for timing, which is monotonic
and not subject to system clock adjustments. The same clock is already used
for per-test duration reporting, so the timeout comparison uses the exact
same measurement — no approximation or separate timing path.

### Timeout marker in `--list-verbose` shows milliseconds

The marker format `[timeout: Xms]` always uses milliseconds regardless of the
duration unit used at registration. This provides a consistent, unambiguous
format. The value is the result of `duration_cast<milliseconds>`.

### No global default timeout

Requirements specify per-test timeouts only. A global `--timeout` CLI flag
would be useful but is not required and can be added as a non-breaking change
later (applying to tests without an explicit timeout).

### No `--timeout` CLI override

Requirements state timeout is registration-time metadata, not toggleable via
CLI. A CLI override would conflict with per-test values and add complexity.
Deferred unless requirements change.

## 6. Feature Changelog

Rewrite of prior design. The previous version used a watchdog `std::jthread`
with `std::stop_token` + `std::condition_variable_any` that called
`std::abort()` on timeout. That entire mechanism is removed.

**Changes from v1:**

- **Removed:** Watchdog thread, `std::jthread`, `std::stop_token`,
  `std::condition_variable_any`, `std::abort()`, `PrintTimeoutMessage`,
  `RunTestWithTimeout`, `RunTestUnguarded`
- **Removed:** Includes `<thread>`, `<atomic>`, `<condition_variable>` from
  `runner.hpp`
- **Added:** Post-test clock check — single `if` after try/catch in `RunTest`
- **Changed:** `Outcome::Timeout` is now reachable in normal execution (was
  previously unreachable / forward-compat only)
- **Changed:** `PrintResult` for `Timeout` is now the primary reporting path
  (was previously dead code)
- **Changed:** `PrintSummary` timeout count is now functional (was previously
  always zero)

**Breaking changes:** None. The `TestMetadata::timeout` field, `Outcome::Timeout`
enumerator, and `Test<Derived>::Timeout()` builder API are identical to v1.
Only the enforcement mechanism changed (internal to Runner).
