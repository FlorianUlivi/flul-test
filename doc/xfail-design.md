# Expected Failure (xfail) — Detailed Design

## 1. Overview

Feature `#XFAIL` introduces per-test expected-failure marking via a fluent
`Test<Derived>` builder. A test marked xfail that fails is reported as XFAIL
(counts as pass for exit code); one that unexpectedly passes is reported as
XPASS (counts as fail). This feature also introduces `TestMetadata`, the
`Outcome` enum, and the `Test<Derived>` builder — all foundational types
reused by `#TAG`, `#SKIP`, and `#TMO`.

## 2. Requirements Reference

From `doc/requirements.md`, feature `#XFAIL`:

- Xfail set via `Test<Derived>` builder's `.ExpectFail()` method
- Test marked xfail that **fails** -> XFAIL (counts as pass)
- Test marked xfail that **passes** -> XPASS (counts as fail)
- Orthogonal to tags and filtering
- `--list` unchanged (CTest safe)
- `--list-verbose` includes `[xfail]` marker
- Set at registration time, not toggleable via CLI
- Summary line includes xfail/xpass counts

## 3. Design Goals

- Introduce `TestMetadata` as the single per-test identity/config struct,
  avoiding repeated `TestEntry`/`TestResult` restructuring for future features
- Replace `passed: bool` with `Outcome` enum in `TestResult` — a one-time
  breaking change that accommodates xfail, skip, and timeout without further
  modifications
- Provide `Test<Derived>` builder with minimal API that commits metadata
  eagerly (no deferred registration)
- Keep the common case (no builder, no xfail) zero-overhead and syntactically
  unchanged
- Preserve CTest compatibility: `--list` output is bare test names only

## 4. C++ Architecture

### 4.1 `TestMetadata` (new: `include/flul/test/test_metadata.hpp`)

```cpp
struct TestMetadata {
    std::string_view suite_name;
    std::string_view test_name;
    bool xfail;

    auto FullName() const -> std::string;
};
```

`suite_name` and `test_name` move here from `TestEntry`. Future features add
fields here: `tags` (`#TAG`), `skip` (`#SKIP`), `timeout` (`#TMO`).

`string_view` members require static storage duration (string literals), same
convention as today. `FullName()` returns the `Suite::Test` format used by
`Filter`, `List`, and `PrintResult`.

### 4.2 `Outcome` (new: `include/flul/test/outcome.hpp`)

```cpp
enum class Outcome { Pass, Fail, XFail, XPass };

constexpr auto IsSuccess(Outcome o) -> bool;
```

Replaces `bool passed` in `TestResult`. Future features extend this enum:
`Skip` (`#SKIP`), `Timeout` (`#TMO`).

`IsSuccess` returns true for `Pass` and `XFail` — these count as success for
exit-code purposes.

### 4.3 `TestEntry` (modified: `include/flul/test/test_entry.hpp`)

```cpp
struct TestEntry {
    TestMetadata metadata;
    std::function<void()> callable;
};
```

Replaces flat `suite_name` / `test_name` with `TestMetadata`. Breaking change.

### 4.4 `TestResult` (modified: `include/flul/test/test_result.hpp`)

```cpp
struct TestResult {
    /* non-owning reference to TestEntry::metadata */
    Outcome outcome;
    std::chrono::nanoseconds duration;
    std::optional<AssertionError> error;
};
```

`metadata` references (does not copy) the `TestMetadata` owned by the
corresponding `TestEntry`. Safe because `Registry` owns `TestEntry` objects
that outlive all `TestResult` instances within `RunAll()`.

`outcome` replaces `bool passed`. Breaking change.

### 4.5 `Test<Derived>` builder (new: `include/flul/test/test.hpp`)

```cpp
template <typename Derived>
class Test {
   public:
    Test(Registry& registry, std::string_view suite_name,
         std::string_view test_name, void (Derived::*method)());

    auto ExpectFail() -> Test&;
};
```

**Eager registration:** The constructor immediately registers the test via
`Registry::Add` and stores a handle to the created entry. Builder methods
(`ExpectFail`) mutate the entry's metadata in place and return `*this` for
chaining.

**Lifetime constraint:** The builder must not outlive the `Register()` call.
No concurrent `Add` calls may occur while a builder's handle is live, because
vector growth could invalidate it. This is naturally satisfied: each builder
chain completes as a single statement before the next `Add`.

**Why eager, not deferred?** Deferred registration (accumulate state, commit
on destruction or `.Done()`) adds destructor logic, move semantics, and risk
of forgetting `.Done()`. Eager registration makes the builder a thin handle
with no ownership responsibilities.

User-facing API:

```cpp
static void Register(Registry& r) {
    // Bulk registration (no xfail):
    AddTests(r, "MySuite", {
        {"TestNormal", &MySuite::TestNormal},
    });

    // Per-test with builder:
    Test<MySuite>(r, "MySuite", "TestBroken", &MySuite::TestBroken)
        .ExpectFail();
}
```

### 4.6 `Registry` changes (modified: `include/flul/test/registry.hpp`)

- `Add` returns a reference to the newly created `TestEntry` (was `void`).
  This is the handle used by `Test<Derived>`.
- New method: `ListVerbose()` — prints one line per test with metadata
  markers in brackets (e.g., `[xfail]`). Tests without markers have no
  bracket suffix.

`Suite<Derived>::AddTests` is unchanged in interface. It calls `Add` in a
loop and discards the returned references.

### 4.7 `Runner` changes (modified: `include/flul/test/runner.hpp`)

`RunTest` determines `Outcome` from the raw pass/fail result and
`metadata.xfail`. The callable itself is unaware of xfail.

`PrintResult` displays the outcome label (PASS, FAIL, XFAIL, XPASS). Error
details are printed for both FAIL and XFAIL (diagnostics are useful even for
expected failures). No error details for XPASS (there is no assertion error).

`PrintSummary` counts each outcome category. All counts are shown (including
zeros) for consistency.

Exit code: success iff all outcomes satisfy `IsSuccess`.

### 4.8 `Run()` changes (modified: `include/flul/test/run.hpp`)

New CLI flag: `--list-verbose`. Execution order:

1. Parse CLI args (filter, list, list-verbose, help)
2. Apply `registry.Filter(pattern)` if `--filter`
3. If `--list`: `registry.List()`, return 0
4. If `--list-verbose`: `registry.ListVerbose()`, return 0
5. Otherwise: `Runner(registry).RunAll()`

### File Map

| File | Change |
|------|--------|
| `include/flul/test/test_metadata.hpp` | **New** — `TestMetadata` struct |
| `include/flul/test/outcome.hpp` | **New** — `Outcome` enum + `IsSuccess` |
| `include/flul/test/test.hpp` | **New** — `Test<Derived>` builder |
| `include/flul/test/test_entry.hpp` | **Modified** — uses `TestMetadata` |
| `include/flul/test/test_result.hpp` | **Modified** — `Outcome`, metadata reference |
| `include/flul/test/registry.hpp` | **Modified** — `Add` returns ref, `ListVerbose` added |
| `include/flul/test/runner.hpp` | **Modified** — `Outcome`-based logic |
| `include/flul/test/run.hpp` | **Modified** — `--list-verbose` flag |

### Namespace

All types remain in `flul::test`. No new namespaces.

## 5. Key Design Decisions

### `Test<Derived>` builder with eager registration

The builder registers the test immediately on construction and returns a
handle to mutate metadata. The alternative — deferred registration where the
builder accumulates state and commits on destruction — adds complexity:
destructor must handle the commit, move semantics must be correct, and
forgetting `.Done()` or relying on destruction order becomes a bug source.

The trade-off is that a partially-configured entry briefly exists in the
registry, but since `Register()` runs to completion before any test
executes, this is harmless.

### `Outcome` enum replaces `bool passed` now, not incrementally

Keeping `bool passed` and adding a separate `xfail_outcome` field would avoid
a breaking change but creates two parallel representations of pass/fail status.
A single `Outcome` enum is cleaner and extends naturally for `#SKIP` and `#TMO`.
Since no backwards compatibility is required, this is the right time.

### No `AddTest` (singular) on `Suite`

The user constructs `Test<MySuite>(registry, ...)` directly. Adding
`Suite::AddTest` that returns a `Test<Derived>` would save one template
argument but adds a forwarding layer with no real benefit.

### `TestResult` references metadata, does not copy

Avoids copying metadata (which will grow with tags vector). The lifetime is
safe because `Registry` outlives `RunAll()`.

### `--list-verbose` introduced with `#XFAIL`, not `#TAG`

`#XFAIL` is the first feature needing `--list-verbose`. When `#TAG` is
implemented, it appends tags to the same bracket group.

### Xfail error details still printed

XFAIL prints the assertion error for diagnostics — developers should see what
the expected failure actually is rather than having it silently swallowed.

## 6. Feature Changelog

Initial version. No prior `doc/xfail-design.md` exists.

**Breaking changes to existing code:**

- `TestEntry` struct layout changes (flat name fields replaced by `TestMetadata`)
- `TestResult` struct layout changes (`bool passed` replaced by `Outcome`,
  flat name fields replaced by metadata reference)
- `Registry::Add` return type changes from `void` to `TestEntry&`
- `Runner` methods must adapt to `Outcome` and `TestMetadata`
- All existing test code that accesses `TestEntry` or `TestResult` fields
  directly must be updated
