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
namespace flul::test {

struct TestMetadata {
    std::string_view suite_name;
    std::string_view test_name;
    bool xfail = false;

    auto FullName() const -> std::string;
};

}  // namespace flul::test
```

**Fields:** `suite_name` and `test_name` move here from `TestEntry`. The `xfail`
flag defaults to `false`. Future features add fields here: `tags` (`#TAG`),
`skip` (`#SKIP`), `timeout` (`#TMO`).

**Ownership:** `string_view` members require static storage duration (string
literals), same convention as today.

**`FullName()`** returns `std::format("{}::{}", suite_name, test_name)`.
Centralizes the formatting used by `Filter`, `List`, `PrintResult`.

### 4.2 `Outcome` (new: `include/flul/test/outcome.hpp`)

```cpp
namespace flul::test {

enum class Outcome { Pass, Fail, XFail, XPass };

}  // namespace flul::test
```

Replaces `bool passed` in `TestResult`. Future features extend this enum:
`Skip` (`#SKIP`), `Timeout` (`#TMO`).

**Exit-code classification:**

| Outcome | Counts as |
|---------|-----------|
| Pass    | pass      |
| Fail    | fail      |
| XFail   | pass      |
| XPass   | fail      |

A free function encapsulates this:

```cpp
constexpr auto IsSuccess(Outcome o) -> bool {
    return o == Outcome::Pass || o == Outcome::XFail;
}
```

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
    std::reference_wrapper<const TestMetadata> metadata;
    Outcome outcome;
    std::chrono::nanoseconds duration;
    std::optional<AssertionError> error;
};
```

- `metadata` is a reference to the `TestEntry::metadata` owned by `Registry`.
  Safe because `Registry` outlives `Runner::RunAll()`.
- `outcome` replaces `bool passed`. Breaking change.

### 4.5 `Test<Derived>` builder (new: `include/flul/test/test.hpp`)

```cpp
namespace flul::test {

template <typename Derived>
class Test {
   public:
    Test(Registry& registry, std::string_view suite_name,
         std::string_view test_name, void (Derived::*method)());

    auto ExpectFail() -> Test&;

   private:
    TestEntry& entry_;  // reference into Registry's vector — see lifetime note
};

}  // namespace flul::test
```

**Construction and lifetime:**

The constructor immediately registers the test into the `Registry` (calls
`Registry::Add`) and stores a reference to the newly created `TestEntry`.
Builder methods like `ExpectFail()` mutate the entry's metadata in place.

```cpp
template <typename Derived>
Test<Derived>::Test(Registry& registry, std::string_view suite_name,
                    std::string_view test_name, void (Derived::*method)())
    : entry_(registry.Add<Derived>(suite_name, test_name, method)) {}

template <typename Derived>
auto Test<Derived>::ExpectFail() -> Test& {
    entry_.metadata.xfail = true;
    return *this;
}
```

**Why eager registration (not deferred)?** Deferred registration would require
the builder to own the metadata and callable, then commit on destruction or an
explicit `.Done()` call. Eager registration is simpler: the entry exists in the
registry immediately, and the builder just pokes at its metadata. No
destructor logic, no moved-from states, no risk of forgetting `.Done()`.

**Why `entry_` reference is safe:** The builder is a temporary or local variable
used only within `Register()`. It does not outlive the `Register()` call.
The `Registry::entries_` vector must not reallocate while a builder holds a
reference. This is guaranteed because builders are used one at a time within
`Register()` and the reference is not stored beyond the builder chain.

**Important:** `Registry::Add` must return `TestEntry&` (reference to the
back-inserted element). If the vector reallocates during a subsequent `Add`
while a previous builder's reference is still live, the reference is
invalidated. This is safe in practice because each builder chain completes
(including all `.ExpectFail()` calls) before the next `Add` call. The design
doc for `#TAG` or implementer should add a note about this constraint.

**Deducing `this` for chaining:** The builder returns `Test&` from `ExpectFail()`.
Deducing `this` is not needed here because `Test<Derived>` is always used as
itself (no further derivation). Simple `Test&` return is sufficient.

### 4.6 `Registry::Add` return type change

```cpp
template <typename S>
    requires std::derived_from<S, Suite<S>> && std::default_initializable<S>
auto Add(std::string_view suite_name, std::string_view test_name,
         void (S::*method)()) -> TestEntry&;
```

Returns a reference to the newly pushed `TestEntry`. This is the only change
to `Registry::Add`'s signature. The implementation is identical except for
using `TestMetadata` and returning `entries_.back()`.

### 4.7 `Suite<Derived>::AddTests` — unchanged semantics

`AddTests` continues to work for bulk registration without the builder. It
calls `Registry::Add` in a loop and discards the returned references. Users
who do not need per-test metadata (xfail, skip, timeout) keep using `AddTests`
with no change to their code (beyond the internal `TestMetadata` restructuring
which is invisible to them).

### 4.8 Per-test registration with builder

For per-test metadata, users call `Test<Derived>` directly in `Register()`:

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

The builder is constructed as a temporary. The `.ExpectFail()` call mutates the
already-registered entry's metadata, then the temporary is destroyed. No
separate `AddTest()` method on `Suite` is needed — the `Test<Derived>`
constructor is the entry point.

### 4.9 `Runner` changes

**`RunTest`** — after calling `entry.callable()`, determines `Outcome`:

```cpp
static auto RunTest(const TestEntry& entry) -> TestResult {
    // ... timing + try/catch as before ...

    // After determining raw pass/fail:
    bool raw_passed = /* true if no exception */;
    Outcome outcome;
    if (entry.metadata.xfail) {
        outcome = raw_passed ? Outcome::XPass : Outcome::XFail;
    } else {
        outcome = raw_passed ? Outcome::Pass : Outcome::Fail;
    }
    // return TestResult with outcome
}
```

The xfail logic is entirely in `RunTest`. The callable itself is unaware of
xfail — it throws or does not throw, same as always.

**`PrintResult`** — maps `Outcome` to display tag:

| Outcome | Display   |
|---------|-----------|
| Pass    | `PASS`    |
| Fail    | `FAIL`    |
| XFail   | `XFAIL`   |
| XPass   | `XPASS`   |

For XFail/XPass, the error details are still printed (useful for diagnostics).

**`PrintSummary`** — counts each outcome category:

```
4 tests, 1 passed, 1 failed, 1 xfail, 1 xpass
```

Zero counts are still shown for consistency and parseability.

**`RunAll` exit code** — uses `IsSuccess`:

```cpp
return std::ranges::all_of(results, [](const TestResult& r) {
    return IsSuccess(r.outcome);
}) ? 0 : 1;
```

### 4.10 `Registry::ListVerbose`

This is introduced by `#XFAIL` (not `#TAG`, since `#TAG` is not yet
implemented). It prints markers for metadata that is set:

```
MySuite::TestNormal
MySuite::TestBroken [xfail]
```

If a test has no metadata markers, no brackets are appended (identical to
`--list` output). Future features append their markers in the same bracket
group: `[xfail, skip]`, `[xfail, timeout: 500ms]`, etc.

### 4.11 `Run()` changes

New CLI flag: `--list-verbose`. Parsed alongside existing flags. Execution
order:

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
| `include/flul/test/test_result.hpp` | **Modified** — `Outcome` + `reference_wrapper` |
| `include/flul/test/registry.hpp` | **Modified** — `Add` returns `TestEntry&`, `ListVerbose` added |
| `include/flul/test/runner.hpp` | **Modified** — `Outcome`-based logic |
| `include/flul/test/run.hpp` | **Modified** — `--list-verbose` flag |

### Namespace

All types remain in `flul::test`. No new namespaces.

## 5. Key Design Decisions

### `Test<Derived>` builder with eager registration

The builder registers the test immediately on construction and returns
references to mutate metadata. The alternative — deferred registration where
the builder accumulates state and commits on destruction — adds complexity:
destructor must handle the commit, move semantics must be correct, and
forgetting `.Done()` or relying on destruction order becomes a bug source.

Eager registration means the builder is just a thin handle to an existing
entry. The trade-off is that a partially-configured entry briefly exists in
the registry, but since `Register()` runs to completion before any test
executes, this is harmless.

### `Outcome` enum replaces `bool passed` now, not incrementally

Keeping `bool passed` and adding a separate `xfail_outcome` field would avoid
a breaking change but creates two parallel representations of pass/fail status.
A single `Outcome` enum is cleaner and extends naturally for `#SKIP` and `#TMO`.
Since no backwards compatibility is required, this is the right time.

### `IsSuccess` as a free function, not a method

`Outcome` is an enum class. Adding methods requires a separate utility. A
`constexpr` free function is the simplest approach and avoids wrapping the enum
in a class.

### No `AddTest` (singular) on `Suite`

The user constructs `Test<MySuite>(registry, ...)` directly. Adding
`Suite::AddTest` that returns a `Test<Derived>` would save one template
argument but adds a forwarding layer with no real benefit. The `Test<Derived>`
constructor is explicit and self-documenting.

### `TestResult` holds `reference_wrapper<const TestMetadata>`

Same rationale as in `#TAG` design: avoids copying metadata (which will grow
with tags vector), and the lifetime is safe because `Registry` outlives
`RunAll()`.

### `--list-verbose` introduced with `#XFAIL`, not `#TAG`

Since `#TAG` is not yet implemented, and `#XFAIL` is the first feature that
needs `--list-verbose`, it is introduced here. When `#TAG` is implemented, it
will append tags to the same bracket group.

### Xfail error details still printed

When a test is XFAIL (expected failure, assertion threw), the error details
are still printed. This helps developers track what the expected failure
actually is, rather than silently swallowing diagnostics. For XPASS, there
is no error to print.

## 6. Feature Changelog

Initial version. No prior `doc/xfail-design.md` exists.

**Breaking changes to existing code:**

- `TestEntry` struct layout changes (flat name fields replaced by `TestMetadata`)
- `TestResult` struct layout changes (`bool passed` replaced by `Outcome`,
  flat name fields replaced by `reference_wrapper<const TestMetadata>`)
- `Registry::Add` return type changes from `void` to `TestEntry&`
- `Runner::RunTest`, `Runner::PrintResult`, `Runner::PrintSummary` must adapt
  to `Outcome` and `TestMetadata`
- `Runner::RunAll` exit-code logic changes from `&TestResult::passed` to
  `IsSuccess(r.outcome)`
- All existing test code that accesses `TestEntry` or `TestResult` fields
  directly must be updated
