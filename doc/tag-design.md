# Tags — Detailed Design

## Overview

Tags (`#TAG`) allow individual tests to be annotated with string labels at
registration time, enabling cross-suite selection and exclusion via `--tag` and
`--exclude-tag` CLI flags. This feature adds a `tags` field to `TestMetadata`,
new filtering methods on `Registry`, and `--list-verbose` output showing tags.

## Requirements Reference

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

## Design Goals

- Keep the registration API minimal: tags are the only new parameter
- Unified filtering pipeline in `Registry` (name filter + tag include/exclude)
- Zero overhead when no tags are used
- `--list` output remains bare test names only
- `TestMetadata` as a distinct value type, owned by `TestEntry`, referenced by
  `TestResult` — enabling future per-test metadata features (#XFAIL, #SKIP,
  #TMO) without further structural changes
- `Registry::Add` returns `TestEntry&` to enable the `Test<Derived>` builder
  pattern used by upcoming features

## C++ Architecture

### Tag Validation

A tag is valid if and only if it is non-empty and every character matches the
pattern `[a-zA-Z0-9_-]` (ASCII alphanumeric, hyphen, underscore). Formally,
the tag must match the regex `^[a-zA-Z0-9_-]+$`.

Invalid tags — including empty strings, whitespace-only strings, and strings
containing characters outside the allowlist (e.g., `[`, `]`, spaces) — cause
the program to print a descriptive diagnostic to `std::cerr` identifying the
offending tag and then call `std::terminate()`. This is a hard abort at
registration time.

Validation is performed inside `Registry::Add`, after deduplication and before
inserting tags into the `TestMetadata::tags` set. Because `AddTests` delegates
to `Add`, all registration paths are covered.

### `TestMetadata` (new: `include/flul/test/test_metadata.hpp`)

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

### `TestEntry` (modified: `include/flul/test/test_entry.hpp`)

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

### `TestResult` (modified: `include/flul/test/test_result.hpp`)

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

### `TestDef` (new: nested in `Suite<Derived>`)

```cpp
struct TestDef {
    std::string_view name;
    void (Derived::*method)();
    std::initializer_list<std::string_view> tags;
};
```

`TestDef` is a plain aggregate that groups a test name, its method pointer, and
an optional tag list into a single registration unit. The `tags` field may be
omitted in aggregate initialization (defaulting to an empty list), making the
no-tags case zero-overhead and syntactically clean.

**`initializer_list` lifetime invariant:** The underlying array backing
`TestDef::tags` lives as long as the full-expression that created the
`TestDef`. Because `AddTests` consumes its `initializer_list<TestDef>`
parameter synchronously — iterating all entries and delegating to
`Registry::Add` before returning — every `TestDef` value (and its `tags`
array) is alive for the entire duration of use. `TestDef` must never be stored
beyond the `AddTests` call.

### `Suite<Derived>::AddTests` (modified signature)

```cpp
static void AddTests(
    Registry& r, std::string_view suite_name,
    std::initializer_list<TestDef> tests);
```

Each `TestDef` carries its own tags, enabling per-test tag assignment:

```cpp
MySuite::AddTests(r, "MySuite", {
    {"Test1", &MySuite::Test1},                     // no tags
    {"Test2", &MySuite::Test2, {"fast", "math"}},   // per-test tags
});
```

`AddTests` iterates the `tests` list and delegates to `Registry::Add` for each
entry, passing `def.name`, `def.method`, and `def.tags`.

### `Registry` (modified: `include/flul/test/registry.hpp`)

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

### `Runner` (modified: `include/flul/test/runner.hpp`)

`RunTest` constructs `TestResult` with a `std::reference_wrapper<const
TestMetadata>` pointing to the entry's metadata, instead of copying
`suite_name` and `test_name` as separate fields.

`PrintResult` and `PrintSummary` access names through
`result.metadata.get().suite_name` and `result.metadata.get().test_name`.

The exit-code logic (`std::ranges::all_of(results, &TestResult::passed)`)
remains unchanged; `passed` is still a direct field on `TestResult`.

Tags do not affect test execution or result reporting — they are purely a
filtering concern.

### `Run()` (modified: `include/flul/test/run.hpp`)

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
| `include/flul/test/suite.hpp` | **Modified** — add `TestDef` aggregate struct; `AddTests` takes `initializer_list<TestDef>` instead of pair + group tags |
| `include/flul/test/registry.hpp` | **Modified** — `Add` returns `TestEntry&`, gains `tags`; `FilterByTag`, `ExcludeByTag`, `ListVerbose`; all accessors go through `entry.metadata` |
| `include/flul/test/runner.hpp` | **Modified** — `RunTest` produces `TestResult` with metadata reference; `PrintResult`/`PrintSummary` access names via `result.metadata.get()` |
| `include/flul/test/run.hpp` | **Modified** — `--tag`, `--exclude-tag`, `--list-verbose` flags |
| `test/tag_test.cpp` | **Modified** — update any direct `TestEntry` field access to use `entry.metadata` |
| `test/tag_adversarial_test.cpp` | **Modified** — same field access updates |

### Namespace

All types remain in `flul::test`. No new namespaces.

## Key Design Decisions

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

### Hard abort on invalid tag content

Tags are validated against `[a-zA-Z0-9_-]+` at registration time. An invalid
tag causes a diagnostic on `std::cerr` followed by `std::terminate()`.

**Why an allowlist:** A denylist (rejecting specific bad characters) is fragile
and invites edge cases. An allowlist is unambiguous, easy to document, and
guarantees that tag strings are safe for all output contexts — including
`--list-verbose` bracket formatting, where characters like `[`, `]`, or
whitespace would produce ambiguous or unparseable output.

**Why abort instead of warning or silently ignoring:** Tags are static labels
set by the developer at registration time, not user input. A bad tag is always
a programming error. Aborting loudly at registration ensures the mistake is
caught immediately during development, rather than producing silently wrong
filtering behavior at runtime. This is consistent with how other invariant
violations (e.g., duplicate test names) are handled in this codebase.

**Trade-off:** Abort prevents any tests from running if a single tag is
invalid. This is intentional — running tests with broken metadata would
undermine the filtering system's reliability.

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

### Per-test tags via `TestDef` aggregate, not builder

Tags are assigned per-test through the `TestDef` struct `{name, method, tags}`.
The previous design used group-level tags on `AddTests` (all tests in one call
shared the same tags) and deferred per-test tagging to a `Test<Derived>`
builder that did not exist. The `TestDef` approach is simpler: it uses plain
aggregate initialization, requires no builder machinery, and provides per-test
granularity directly. The `tags` field is optional in aggregate init, so the
no-tags case remains clean.

**Trade-off:** The `TestDef` approach does not support group-level tags
(applying one tag set to many tests in a single call). If needed, users repeat
the tag in each `TestDef`. This is acceptable because tag sets are small and
explicit repetition is clearer than implicit sharing.

### `--list-verbose` omits brackets for untagged tests

No trailing `[]` for tests without tags. Makes `--list-verbose` identical to
`--list` for untagged suites, reducing visual noise.

### Tag filtering applied after name filtering

Matches requirements (`--filter` first, tag second) and is efficient: name
filtering narrows the set before tag filtering iterates.

## Feature Changelog

Initial version. No prior `doc/tag-design.md` exists.

**v1.1** — Added `#RAND` shuffle step to the `Run()` execution order.
Listing steps remain before shuffle, ensuring `--list`/`--list-verbose`
are unaffected by randomization.

**v1.2** — Corrective refactoring to introduce `TestMetadata` struct
(addresses KI-001, KI-002). The implementation diverged from the documented
architecture: `TestEntry` held raw fields (`suite_name`, `test_name`, `tags`,
`HasTag`) directly, `TestResult` duplicated `suite_name`/`test_name` as
separate fields, and `Registry::Add` returned `void`. This version aligns the
design with the architecture diagram and `doc/requirements.md` design
decisions.

Changes from v1.1:

- Requirements Reference: added `TestMetadata` design decision reference
- Design Goals: added goals for `TestMetadata` extraction and `Registry::Add` return type
- Tag Validation (new): `TestMetadata` is a new file; fields: `suite_name`, `test_name`, `tags`, `HasTag`
- TestEntry (new): modified to contain `TestMetadata metadata` instead of raw fields; `HasTag` removed
- TestResult (new): holds `std::reference_wrapper<const TestMetadata>` instead of separate fields; lifetime invariant documented
- Registry: `Add` return type changed from `void` to `TestEntry&`; all filter/list methods documented
- Runner (new): `RunTest` constructs `TestResult` with metadata reference; accessors updated
- File map: expanded to include `test_entry.hpp`, `test_result.hpp`, `runner.hpp`, and test files
- Key Design Decisions: added three new decisions (`TestMetadata` as separate struct, `TestResult` references metadata, `Registry::Add` returns `TestEntry&`)

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

**v1.3** — Tag content validation (addresses KI-004).

Changes from v1.2:

- Tag Validation (new section): specifies the `[a-zA-Z0-9_-]+` allowlist,
  abort-on-invalid behavior via `std::cerr` + `std::terminate()`, and
  validation call site in `Registry::Add`
- Key Design Decisions: added "Hard abort on invalid tag content"

No breaking changes to existing valid code. Previously accepted invalid tags
(empty, whitespace, bracket-containing) will now cause a hard abort.

**v1.4** — Per-test tags via `TestDef` aggregate (addresses KI-003).

The previous design assigned tags at group level on `AddTests` and deferred
per-test tagging to a `Test<Derived>` builder that did not exist, making
per-test tagging impossible. This version replaces the group-level mechanism
with a `TestDef` aggregate struct that carries per-test tags directly.

Changes from v1.3:

- `TestDef` (new): aggregate struct — fields `name`, `method`, `tags`;
  `initializer_list` lifetime invariant documented
- `Suite<Derived>::AddTests`: signature changed from
  `initializer_list<pair<string_view, method>> + trailing tags` to
  `initializer_list<TestDef>`; usage example added; removed reference to
  non-existent `Test<Derived>` builder for tags
- File map: `suite.hpp` entry updated to reflect `TestDef` struct and new
  `AddTests` signature
- Key Design Decisions: replaced "Tags on `AddTests` are group-level" with
  "Per-test tags via `TestDef` aggregate, not builder"

**Breaking changes:**

- `Suite<Derived>::AddTests` signature changes: the `tests` parameter type
  changes from `initializer_list<pair<string_view, void (Derived::*)()>>` to
  `initializer_list<TestDef>`. The trailing `tags` parameter is removed. All
  call sites must be updated to use `TestDef` aggregate syntax
- Group-level tags (one tag set shared across all tests in an `AddTests` call)
  are no longer supported; tags must be specified per `TestDef` entry
