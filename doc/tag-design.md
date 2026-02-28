# Tags — Detailed Design

## 1. Overview

Tags (`#TAG`) allow individual tests to be annotated with string labels at
registration time, enabling cross-suite selection and exclusion via `--tag` and
`--exclude-tag` CLI flags. This feature adds a `tags` field to `TestMetadata`,
new filtering methods on `Registry`, and `--list-verbose` output showing tags.

## 2. Requirements Reference

From `doc/requirements.md`, feature `#TAG`:

- Tags assigned at registration time as optional argument to `Register()`
- Multiple tags per test
- `--tag <tag>` runs only tests carrying that tag (repeatable; OR-combined)
- `--exclude-tag <tag>` skips tests carrying that tag (repeatable)
- `--exclude-tag` takes precedence over `--tag`
- Both compose with `--filter`: name filter first, tag filter second
- `--list` unchanged (CTest safe)
- `--list-verbose` shows tags: `SuiteName::TestName [tag1, tag2]`

From `doc/requirements.md`, design decisions:

- `TestMetadata` value type holds test identity and configuration (names, tags,
  flags such as xfail or timeout); owned by `TestEntry`, borrowed by
  `TestResult` via `std::reference_wrapper<const TestMetadata>`

## 3. Design Goals

- Keep the registration API minimal: tags are the only new parameter
- Unified filtering pipeline in `Registry` (name filter + tag include/exclude)
- Zero overhead when no tags are used
- `--list` output remains bare test names only
- `TestMetadata` as a distinct value type, owned by `TestEntry`, referenced by
  `TestResult` — enabling future per-test metadata features (#XFAIL, #SKIP,
  #TMO) without further structural changes
- `Registry::Add` returns `TestEntry&` to enable the `Test<Derived>` builder
  pattern used by upcoming features

## 4. C++ Architecture

### 4.1 `TestMetadata` (new: `include/flul/test/test_metadata.hpp`)

```cpp
struct TestMetadata {
    std::string_view suite_name;
    std::string_view test_name;
    std::set<std::string_view> tags;

    auto HasTag(std::string_view tag) const -> bool;
};
```

`TestMetadata` is the single value type for per-test identity and
configuration. It owns the test's name pair and tag set. Future features add
fields here (e.g., `bool xfail` for #XFAIL, `bool skip` for #SKIP,
`std::optional<std::chrono::milliseconds> timeout` for #TMO).

`suite_name` and `test_name` are `string_view` pointing to static-duration
storage (string literals). `tags` uses `std::set<std::string_view>` to
guarantee uniqueness and sorted order for deterministic `--list-verbose`
output. The set is empty (no heap allocation on common implementations) when
no tags are provided.

`HasTag` returns whether the given tag is present in the set.

### 4.2 `TestEntry` (modified: `include/flul/test/test_entry.hpp`)

```cpp
struct TestEntry {
    TestMetadata metadata;
    std::function<void()> callable;
};
```

`TestEntry` contains `TestMetadata` by value (composition). The `suite_name`,
`test_name`, `tags`, and `HasTag()` fields that currently live directly on
`TestEntry` are removed; they move into `TestMetadata`.

All code that currently accesses `entry.suite_name`, `entry.test_name`,
`entry.tags`, or `entry.HasTag(...)` must go through `entry.metadata` instead
(e.g., `entry.metadata.suite_name`, `entry.metadata.HasTag(...)`).

### 4.3 `TestResult` (modified: `include/flul/test/test_result.hpp`)

```cpp
struct TestResult {
    std::reference_wrapper<const TestMetadata> metadata;
    bool passed;
    std::chrono::nanoseconds duration;
    std::optional<AssertionError> error;
};
```

`TestResult` replaces its `suite_name` and `test_name` fields with a single
`std::reference_wrapper<const TestMetadata>` that borrows the metadata from
the corresponding `TestEntry` in the `Registry`.

**Lifetime invariant:** The `Registry` that owns the `TestEntry` (and thus the
`TestMetadata`) must outlive all `TestResult` instances. This is guaranteed
because `Registry` is created in `main()` and `TestResult` vectors are local
to `Runner::RunAll()`.

All code that currently accesses `result.suite_name` or `result.test_name`
must go through `result.metadata.get()` instead (e.g.,
`result.metadata.get().suite_name`).

### 4.4 `Suite<Derived>::AddTests` (modified signature)

```cpp
static void AddTests(
    Registry& r, std::string_view suite_name,
    std::initializer_list<std::pair<std::string_view, void (Derived::*)()>> tests,
    std::initializer_list<std::string_view> tags = {});
```

Tags are a trailing parameter with default `{}`. All tests registered in a
single `AddTests` call share the same tag set. This matches the common case:
tags are a suite-level or logical-group concern, not per-method.

For per-test tags, users use the `Test<Derived>` builder (from `#XFAIL`)
with a `.Tag("x")` method.

### 4.5 `Registry` (modified: `include/flul/test/registry.hpp`)

Methods and signature changes:

```cpp
template <typename S>
    requires std::derived_from<S, Suite<S>> && std::default_initializable<S>
auto Add(std::string_view suite_name, std::string_view test_name,
         void (S::*method)(),
         std::initializer_list<std::string_view> tags = {}) -> TestEntry&;

void FilterByTag(std::span<const std::string_view> include_tags);
void ExcludeByTag(std::span<const std::string_view> exclude_tags);
void ListVerbose() const;
```

**`Add` returns `TestEntry&`** — a reference to the newly appended entry in
the internal vector. This enables the `Test<Derived>` builder pattern: the
builder stores this reference and mutates `metadata` fields in place (used by
#XFAIL's `.ExpectFail()`, #SKIP's `.Skip()`, #TMO's `.Timeout()`). Vector
reallocation safety: each builder chain completes (temporary destroyed) before
the next `Add` call, so no dangling references occur.

`Add` constructs a `TestMetadata` value from the parameters, then pushes a
`TestEntry{metadata, callable}`. Tag deduplication with a warning on
duplicates is preserved from the current implementation.

**`FilterByTag`** — erases entries whose `metadata` does not carry any of the
given tags (OR semantics). No-op if `include_tags` is empty. Applied after
`Filter`.

**`ExcludeByTag`** — erases entries whose `metadata` carries any of the given
tags. Applied after `FilterByTag`. This ordering guarantees
exclude-takes-precedence: a test matching both `--tag` and `--exclude-tag` is
included by `FilterByTag` then removed by `ExcludeByTag`.

**`ListVerbose`** — prints one line per test. Reads names and tags from
`entry.metadata`. Tags are appended in brackets; tests without tags have no
bracket suffix.

**`List`** — reads `entry.metadata.suite_name` and `entry.metadata.test_name`.

**`Filter`** — reads `entry.metadata.suite_name` and
`entry.metadata.test_name` to form the qualified name for pattern matching.

### 4.6 `Runner` (modified: `include/flul/test/runner.hpp`)

`RunTest` constructs `TestResult` with a `std::reference_wrapper<const
TestMetadata>` pointing to the entry's metadata, instead of copying
`suite_name` and `test_name` as separate fields.

`PrintResult` and `PrintSummary` access names through
`result.metadata.get().suite_name` and `result.metadata.get().test_name`.

The exit-code logic (`std::ranges::all_of(results, &TestResult::passed)`)
remains unchanged; `passed` is still a direct field on `TestResult`.

Tags do not affect test execution or result reporting — they are purely a
filtering concern.

### 4.7 `Run()` (modified: `include/flul/test/run.hpp`)

New CLI flags: `--tag <tag>` (repeatable), `--exclude-tag <tag>` (repeatable),
`--list-verbose`.

Execution order in `Run()`:

1. Parse all CLI args
2. Validate `--seed` value if present (fail fast on invalid input)
3. `registry.Filter(pattern)` if `--filter`
4. `registry.FilterByTag(include_tags)` if any `--tag` flags
5. `registry.ExcludeByTag(exclude_tags)` if any `--exclude-tag` flags
6. If `--list`: `registry.List()`, return 0
7. If `--list-verbose`: `registry.ListVerbose()`, return 0
8. If `--randomize` or `--seed`: print `[seed: N]`, `registry.Shuffle(seed)` (see `doc/rand-design.md`)
9. `Runner(registry).RunAll()`

`--list` and `--list-verbose` respect all filters — they show only the tests
that would run.

### File Map

| File | Change |
|------|--------|
| `include/flul/test/test_metadata.hpp` | **New** — `TestMetadata` struct with `suite_name`, `test_name`, `tags`, `HasTag` |
| `include/flul/test/test_entry.hpp` | **Modified** — replace raw fields with `TestMetadata metadata`; remove `HasTag` |
| `include/flul/test/test_result.hpp` | **Modified** — replace `suite_name`/`test_name` with `reference_wrapper<const TestMetadata>`; remove `#include <string_view>` |
| `include/flul/test/suite.hpp` | **Modified** — `AddTests` gains `tags` param |
| `include/flul/test/registry.hpp` | **Modified** — `Add` returns `TestEntry&`, gains `tags`; `FilterByTag`, `ExcludeByTag`, `ListVerbose`; all accessors go through `entry.metadata` |
| `include/flul/test/runner.hpp` | **Modified** — `RunTest` produces `TestResult` with metadata reference; `PrintResult`/`PrintSummary` access names via `result.metadata.get()` |
| `include/flul/test/run.hpp` | **Modified** — `--tag`, `--exclude-tag`, `--list-verbose` flags |
| `test/tag_test.cpp` | **Modified** — update any direct `TestEntry` field access to use `entry.metadata` |
| `test/tag_adversarial_test.cpp` | **Modified** — same field access updates |

### Namespace

All types remain in `flul::test`. No new namespaces.

## 5. Key Design Decisions

### `TestMetadata` as a separate struct

Per `doc/requirements.md` design decisions, `TestMetadata` is the central
value type for per-test identity and configuration. Extracting it from
`TestEntry` into its own struct enables `TestResult` to reference it without
duplicating fields, and provides a single extension point for future per-test
metadata (#XFAIL, #SKIP, #TMO) without touching `TestEntry` or `TestResult`
structure again.

**Trade-off:** Adds one level of indirection (`entry.metadata.suite_name`
instead of `entry.suite_name`). This is a minor verbosity cost that pays off
by avoiding field duplication across `TestEntry` and `TestResult`, and by
making future metadata additions a single-struct change.

### `TestResult` references `TestMetadata` instead of copying fields

Using `std::reference_wrapper<const TestMetadata>` avoids copying name strings
and tags into every `TestResult`. The lifetime guarantee is straightforward:
`Registry` outlives `Runner::RunAll()`.

**Trade-off:** `TestResult` is no longer self-contained — it cannot outlive
the `Registry`. This is acceptable because results are local to `RunAll()` and
never returned or stored externally.

### `Registry::Add` returns `TestEntry&`

Returning a reference to the just-added entry enables the `Test<Derived>`
builder pattern (needed by #XFAIL, #SKIP, #TMO). The builder stores this
reference and mutates `metadata` fields in place. Vector reallocation is safe
because each builder chain is a single expression that completes before the
next `Add` call.

**Trade-off:** Callers must not store the returned reference across `Add`
calls. This is enforced by convention (builder is a temporary) not by the type
system. Acceptable because the builder pattern is the only consumer.

### Tags stored as `std::set<std::string_view>`

Using `std::set` guarantees uniqueness at insertion time and provides sorted
iteration for deterministic `--list-verbose` output. The current
implementation already uses `std::set` on `TestEntry`; this design preserves
that choice by moving it into `TestMetadata`.

**Trade-off:** `std::set` has higher per-element overhead than
`std::vector` for small tag counts. Acceptable because tag counts are tiny
(typically 1-3 per test) and the sorted/unique properties are valuable.

### Tags as `string_view` with static-duration requirement

Tags are string literals in practice (e.g., `{"fast", "math"}`). Storing
`string_view` avoids heap allocation when tags are present and keeps
`TestMetadata` cheap to copy. The static-duration requirement is the same
convention already used for `suite_name` and `test_name`.

**Trade-off:** Users cannot use dynamically constructed tag strings. Acceptable
because tags are fixed labels, not computed values.

### Separate `FilterByTag` / `ExcludeByTag` instead of unified filter object

A unified `TestFilter` object would be more composable but more complex. Three
in-place mutation calls (`Filter`, `FilterByTag`, `ExcludeByTag`) applied in
sequence are simple, explicit, and sufficient for the CLI-driven use case.

**Trade-off:** If future features need to reset or reapply filters, in-place
mutation becomes awkward. Not in requirements; straightforward to refactor if
needed.

### Tags on `AddTests` are group-level, not per-test

Per-test tags in `AddTests` would require a 3-tuple `{name, method, tags}`
which is verbose and rarely needed. The `Test<Derived>` builder covers
per-test tags via `.Tag("x")`.

### `--list-verbose` omits brackets for untagged tests

No trailing `[]` for tests without tags. Makes `--list-verbose` identical to
`--list` for untagged suites, reducing visual noise.

### Tag filtering applied after name filtering

Matches requirements (`--filter` first, tag second) and is efficient: name
filtering narrows the set before tag filtering iterates.

## 6. Feature Changelog

Initial version. No prior `doc/tag-design.md` exists.

**v1.1** — Added `#RAND` shuffle step (step 8) to section 4.4 execution order.
Listing steps (6-7) remain before shuffle, ensuring `--list`/`--list-verbose`
are unaffected by randomization.

**v1.2** — Corrective refactoring to introduce `TestMetadata` struct
(addresses KI-001, KI-002). The implementation diverged from the documented
architecture: `TestEntry` held raw fields (`suite_name`, `test_name`, `tags`,
`HasTag`) directly, `TestResult` duplicated `suite_name`/`test_name` as
separate fields, and `Registry::Add` returned `void`. This version aligns the
design with the architecture diagram and `doc/requirements.md` design
decisions.

Changes from v1.1:

- Section 2: added requirements reference to the `TestMetadata` design
  decision in `doc/requirements.md`
- Section 3: added two design goals for `TestMetadata` extraction and
  `Registry::Add` return type
- Section 4.1: changed from **Modified** to **New** — `TestMetadata` is a new
  file (`include/flul/test/test_metadata.hpp`), not a modification of an
  existing one. Fields: `suite_name`, `test_name`, `tags` (as
  `std::set<std::string_view>`), plus `HasTag` method
- Section 4.2 (new): `TestEntry` modified to contain `TestMetadata metadata`
  instead of raw fields; `HasTag` removed from `TestEntry`
- Section 4.3 (new): `TestResult` modified to hold
  `std::reference_wrapper<const TestMetadata>` instead of separate
  `suite_name`/`test_name` fields. Lifetime invariant documented
- Section 4.5: `Registry::Add` return type changed from `void` to
  `TestEntry&`; all filter/list methods documented to access fields through
  `entry.metadata`
- Section 4.6 (new): `Runner` modifications — `RunTest` constructs
  `TestResult` with metadata reference; `PrintResult`/`PrintSummary` access
  names through `result.metadata.get()`
- File map: expanded to include `test_entry.hpp`, `test_result.hpp`,
  `runner.hpp`, and test files
- Section 5: added three new design decisions (`TestMetadata` as separate
  struct, `TestResult` references metadata, `Registry::Add` returns
  `TestEntry&`)

**Breaking changes to existing code:**

- `TestEntry` field layout changes: `suite_name`, `test_name`, `tags` move
  into `TestEntry::metadata`; direct field access like `entry.suite_name`
  becomes `entry.metadata.suite_name`
- `TestEntry::HasTag()` is removed; use `entry.metadata.HasTag()` instead
- `TestResult` field layout changes: `suite_name` and `test_name` are removed;
  replaced by `metadata` (`reference_wrapper<const TestMetadata>`). Access
  like `result.suite_name` becomes `result.metadata.get().suite_name`
- `Registry::Add` return type changes from `void` to `TestEntry&`. Callers
  that ignore the return value are unaffected
- All test files that construct `TestEntry` or `TestResult` directly, or
  access their fields, must be updated
