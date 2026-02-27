# Skip — Detailed Design

## 1. Overview

Feature `#SKIP` allows individual tests to be marked as skipped at registration
time via the `Test<Derived>` builder's `.Skip()` method. Skipped tests are not
executed, are reported as `[ SKIP ]` in output, and do not affect the exit code.
This feature builds on the `TestMetadata`, `Outcome`, and `Test<Derived>` builder
infrastructure introduced by `#XFAIL`.

## 2. Requirements Reference

From `doc/requirements.md`, feature `#SKIP`:

- Skip set via `Test<Derived>` builder's `.Skip()` method
- A skipped test is not executed
- Reported as **SKIP**, counts as neither pass nor fail for exit code
- Orthogonal to tags and filtering: applied after filtering, so a filtered-out
  test is never reported as skipped
- `--list` unchanged (CTest safe, bare names only)
- `--list-verbose` includes `[skip]` marker
- Set at registration time, not toggleable via CLI
- Summary line includes skip count alongside pass/fail/xfail/xpass

## 3. Design Goals

- Minimal delta over `#XFAIL`: one new field on `TestMetadata`, one new
  `Outcome` value, one new builder method
- Skipped tests produce zero side effects: no fixture construction, no
  `SetUp`/`TearDown`, no callable invocation
- Skip is orthogonal to xfail: a test can be both `xfail` and `skip`; skip
  takes precedence (the test is not run, so xfail is moot)
- Preserve CTest compatibility: `--list` output is bare test names only
- Summary line always shows the skip count (even when zero) for consistency

## 4. C++ Architecture

### 4.1 `TestMetadata` (modified: `include/flul/test/test_metadata.hpp`)

New field:

```cpp
struct TestMetadata {
    // ...existing fields (suite_name, test_name, xfail, tags)...
    bool skip = false;
};
```

Default `false` ensures existing tests and `AddTests` bulk registration are
unaffected.

### 4.2 `Outcome` (modified: `include/flul/test/outcome.hpp`)

New enumerator:

```cpp
enum class Outcome { Pass, Fail, XFail, XPass, Skip };
```

`IsSuccess` updated: `Skip` is considered success (it must not cause non-zero
exit). Specifically, `IsSuccess(Outcome::Skip)` returns `true`.

### 4.3 `Test<Derived>` builder (modified: `include/flul/test/test.hpp`)

New method:

```cpp
template <typename Derived>
class Test {
   public:
    // ...existing constructor, ExpectFail()...

    auto Skip() -> Test&;
};
```

`Skip()` sets `metadata.skip = true` on the already-registered `TestEntry` and
returns `*this` for chaining. Same eager-mutation pattern as `ExpectFail()`.

Chaining `Skip()` with `ExpectFail()` is legal: skip takes precedence at
runtime (the callable is never invoked, so xfail status is irrelevant).

### 4.4 `Runner` changes (modified: `include/flul/test/runner.hpp`)

`RunTest` checks `entry.metadata.skip` **before** invoking the callable:

```
if metadata.skip:
    return TestResult with outcome = Outcome::Skip, duration = 0, no error
```

No fixture is constructed. No `SetUp`/`TearDown` runs. Duration is zero
(represented as `std::chrono::nanoseconds{0}`).

`PrintResult` maps `Outcome::Skip` to the label `SKIP`. No error details are
printed for skipped tests.

`PrintSummary` counts skip alongside other outcomes. Format:

```
N tests, P passed, F failed, S skipped, X xfail, U xpass
```

All counts shown (including zeros) for consistency.

Exit code: unchanged logic — success iff all outcomes satisfy `IsSuccess`.
Since `IsSuccess(Skip) == true`, skipped tests never cause failure.

### 4.5 `Registry::ListVerbose` (modified: `include/flul/test/registry.hpp`)

`ListVerbose` appends `[skip]` for tests with `metadata.skip == true`. When a
test has both skip and other markers (xfail, tags), all are shown in the bracket
group. Example:

```
MySuite::TestBroken [skip]
MySuite::TestFlaky [xfail]
MySuite::TestTagged [fast, math]
MySuite::TestSkippedTagged [skip, slow]
```

`List()` is unchanged — bare test names only.

### 4.6 `Run()` (unchanged: `include/flul/test/run.hpp`)

No new CLI flags for `#SKIP`. The execution order in `Run()` is unchanged from
`#TAG`/`#XFAIL`:

1. Parse all CLI args
2. Validate `--seed` if present
3. `registry.Filter(pattern)` if `--filter`
4. `registry.FilterByTag(include_tags)` if any `--tag` flags
5. `registry.ExcludeByTag(exclude_tags)` if any `--exclude-tag` flags
6. If `--list`: `registry.List()`, return 0
7. If `--list-verbose`: `registry.ListVerbose()`, return 0
8. If `--randomize` or `--seed`: print seed, `registry.Shuffle(seed)`
9. `Runner(registry).RunAll()`

Skipped tests survive filtering and listing — they appear in `--list` and
`--list-verbose` output. They are only short-circuited at execution time in
`Runner::RunTest`.

### 4.7 Interaction with tag filtering

A test marked skip that is also excluded by `--exclude-tag` is removed from the
test set entirely — it does not appear in output or counts. A test marked skip
that survives filtering appears in the run and is reported as `[ SKIP ]`.

This matches the requirement: "applied after filtering, so a filtered-out test
is never reported as skipped."

### File Map

| File | Change |
|------|--------|
| `include/flul/test/test_metadata.hpp` | **Modified** — `skip` field |
| `include/flul/test/outcome.hpp` | **Modified** — `Skip` enumerator, `IsSuccess` updated |
| `include/flul/test/test.hpp` | **Modified** — `Skip()` builder method |
| `include/flul/test/runner.hpp` | **Modified** — skip check in `RunTest`, `PrintResult`, `PrintSummary` |
| `include/flul/test/registry.hpp` | **Modified** — `ListVerbose` includes `[skip]` marker |

### Namespace

All types remain in `flul::test`. No new namespaces.

## 5. Key Design Decisions

### Skip check before callable invocation, not via a special callable

The alternative — replacing the callable with a no-op at registration time —
would avoid the runtime check but loses information: the original callable is
gone, making it impossible to "unskip" programmatically in the future. A simple
`if (metadata.skip)` branch in `RunTest` is trivially cheap and preserves the
original callable.

### Skip takes precedence over xfail

A test marked both skip and xfail is reported as `Skip`, not `XFail` or `XPass`.
The callable is never invoked, so xfail classification is meaningless. This
avoids a confusing "expected failure of a test that didn't run" state.

### `IsSuccess(Skip)` returns true

Skip is explicitly neutral — it should never cause the test binary to exit
non-zero. The alternative (treating skip as a distinct non-success state) would
require changing the exit-code logic to exclude skips, adding complexity for no
benefit.

### Duration is zero for skipped tests

No work is done, so reporting zero duration is accurate and avoids timing
overhead. The alternative — omitting duration entirely — would require
`std::optional<nanoseconds>` which complicates `PrintResult` and `FormatDuration`
for no user benefit.

### Skipped tests appear in `--list` and `--list-verbose`

Skipped tests are part of the registered test set. They should be visible in
listings so users know they exist. Hiding them from `--list` would break CTest
discovery (CTest would not know about them) and hiding them from
`--list-verbose` would reduce discoverability.

### No `--skip` CLI flag

Requirements specify skip as registration-time metadata only. A `--skip` CLI
flag would overlap with `--exclude-tag` (users can tag tests and exclude by tag).
Adding it later is non-breaking if needed.

## 6. Feature Changelog

Initial version. No prior `doc/skip-design.md` exists.

**Breaking changes to existing code:** None beyond those already introduced by
`#XFAIL`. The `skip` field on `TestMetadata` has a default value (`false`) and
the new `Outcome::Skip` enumerator is additive. `Runner` and `ListVerbose`
changes are internal implementation updates.
