# Tags — Detailed Design

## 1. Overview

Tags (`#TAG`) allow individual tests to be annotated with string labels at
registration time, enabling cross-suite selection and exclusion via `--tag` and
`--exclude-tag` CLI flags. This feature introduces `TestMetadata` as the central
value type for per-test identity and configuration, replaces the flat fields in
`TestEntry` and `TestResult`, and adds `--list-verbose` output.

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

## 3. Design Goals

- Introduce `TestMetadata` as the single source of per-test identity and config,
  preparing the ground for `#XFAIL`, `#SKIP`, and `#TMO` without further refactoring
- Keep the registration API minimal: tags are the only new parameter
- Unified filtering pipeline in `Registry` (name filter + tag include/exclude)
- Zero overhead when no tags are used
- `--list` output remains bare test names only

## 4. C++ Architecture

### 4.1 `TestMetadata` (new: `include/flul/test/test_metadata.hpp`)

```cpp
namespace flul::test {

struct TestMetadata {
    std::string_view suite_name;
    std::string_view test_name;
    std::vector<std::string_view> tags;

    auto FullName() const -> std::string {
        return std::format("{}::{}", suite_name, test_name);
    }

    auto HasTag(std::string_view tag) const -> bool {
        return std::ranges::find(tags, tag) != tags.end();
    }
};

}  // namespace flul::test
```

**Ownership:** `tags` stores `string_view` elements. Like `suite_name` and
`test_name`, these must point to storage with static duration (string literals).
This is already the convention for names and extends naturally to tags. The
`vector` is empty (no heap allocation) when no tags are provided.

**Why a separate type:** `TestMetadata` groups identity fields that will grow
with `#XFAIL` (bool), `#SKIP` (bool), and `#TMO` (optional duration). Extracting
it now avoids changing `TestEntry` and `TestResult` repeatedly.

### 4.2 `TestEntry` (modified: `include/flul/test/test_entry.hpp`)

```cpp
struct TestEntry {
    TestMetadata metadata;
    std::function<void()> callable;
};
```

Replaces the flat `suite_name` / `test_name` fields with a `TestMetadata` value.
This is a breaking change to `TestEntry`.

### 4.3 `TestResult` (modified: `include/flul/test/test_result.hpp`)

```cpp
struct TestResult {
    std::reference_wrapper<const TestMetadata> metadata;
    bool passed;
    std::chrono::nanoseconds duration;
    std::optional<AssertionError> error;
};
```

Replaces flat `suite_name` / `test_name` with a `reference_wrapper` to the
`TestMetadata` owned by the corresponding `TestEntry`. This avoids copying
metadata and keeps `TestResult` lightweight. The reference is safe because
`Registry` owns `TestEntry` objects that outlive all `TestResult` instances
(both live within a single `RunAll()` call, and `Registry` outlives `Runner`).

### 4.4 `Suite<Derived>::AddTests` (modified signature)

```cpp
template <typename Derived>
class Suite {
public:
    // ...existing...

    static void AddTests(
        Registry& r, std::string_view suite_name,
        std::initializer_list<std::pair<std::string_view, void (Derived::*)()>> tests,
        std::initializer_list<std::string_view> tags = {});
};
```

Tags are a trailing parameter with default `{}`. All tests registered in a
single `AddTests` call share the same tag set. This matches the common case:
tags are a suite-level or logical-group concern, not per-method.

**Why not per-test tags in `AddTests`?** It would require a 3-tuple
`{name, method, tags}` which is verbose and rarely needed. For the rare case
of per-test tags, users call `Registry::Add` directly. The `Test<Derived>`
builder (planned for `#XFAIL`) will also support per-test tags via `.Tag("x")`.

**Why `initializer_list<string_view>` instead of variadic or concepts?**
It is the simplest spelling that supports zero-to-many string literals:
`AddTests(r, "S", {{"T", &S::T}}, {"fast", "math"})`. No templates, no
forwarding, no ambiguity.

### 4.5 `Registry` (modified: `include/flul/test/registry.hpp`)

Changes to `Registry`:

```cpp
class Registry {
public:
    template <typename S>
        requires std::derived_from<S, Suite<S>> && std::default_initializable<S>
    void Add(std::string_view suite_name, std::string_view test_name,
             void (S::*method)(),
             std::initializer_list<std::string_view> tags = {});

    void Filter(std::string_view pattern);
    void FilterByTag(std::span<const std::string_view> include_tags);
    void ExcludeByTag(std::span<const std::string_view> exclude_tags);

    void List() const;
    void ListVerbose() const;

    [[nodiscard]] auto Tests() const -> std::span<const TestEntry>;

private:
    std::vector<TestEntry> entries_;
};
```

**`Filter`** — unchanged semantics: substring match on `FullName()`, in-place
erase. Applied first.

**`FilterByTag`** — erases entries that do not carry any of the given tags
(OR semantics). If `include_tags` is empty, no-op. Applied after `Filter`.

**`ExcludeByTag`** — erases entries that carry any of the given tags. Applied
after `FilterByTag`. This ordering guarantees exclude-takes-precedence: a test
matching both `--tag` and `--exclude-tag` is included by `FilterByTag` then
removed by `ExcludeByTag`.

**`ListVerbose`** — prints one line per test:
```
SuiteName::TestName [tag1, tag2]
```
If a test has no tags, the bracket suffix is omitted:
```
SuiteName::TestName
```

**`List`** — unchanged. Bare `SuiteName::TestName`, no tags.

### 4.6 `Run()` (modified: `include/flul/test/run.hpp`)

New CLI flags parsed in `Run()`:

| Flag | Accumulation | Passed to |
|------|-------------|-----------|
| `--tag <tag>` | Collect into `vector<string_view>` | `Registry::FilterByTag` |
| `--exclude-tag <tag>` | Collect into `vector<string_view>` | `Registry::ExcludeByTag` |
| `--list-verbose` | Boolean flag | `Registry::ListVerbose` |

**Execution order in `Run()`:**

1. Parse all CLI args into local variables (filter pattern, tag lists, flags)
2. Apply `registry.Filter(pattern)` if `--filter` was given
3. Apply `registry.FilterByTag(include_tags)` if any `--tag` flags
4. Apply `registry.ExcludeByTag(exclude_tags)` if any `--exclude-tag` flags
5. If `--list`: call `registry.List()`, return 0
6. If `--list-verbose`: call `registry.ListVerbose()`, return 0
7. Otherwise: create `Runner`, call `RunAll()`

This means `--list` and `--list-verbose` respect `--filter`, `--tag`, and
`--exclude-tag` — they show only the tests that would run.

### 4.7 `Runner` (modified: `include/flul/test/runner.hpp`)

- `RunTest` returns `TestResult` with `metadata` as `reference_wrapper` to the
  `TestEntry::metadata` instead of copying flat fields
- `PrintResult` reads names from `result.metadata.get()`
- `PrintSummary` unchanged in logic

### 4.8 Namespace

All types remain in `flul::test`. No new namespaces.

### File Map

| File | Change |
|------|--------|
| `include/flul/test/test_metadata.hpp` | **New** — `TestMetadata` struct |
| `include/flul/test/test_entry.hpp` | **Modified** — uses `TestMetadata` |
| `include/flul/test/test_result.hpp` | **Modified** — `reference_wrapper<const TestMetadata>` |
| `include/flul/test/suite.hpp` | **Modified** — `AddTests` gains `tags` param |
| `include/flul/test/registry.hpp` | **Modified** — `Add` gains `tags`, new methods |
| `include/flul/test/run.hpp` | **Modified** — new CLI flags |
| `include/flul/test/runner.hpp` | **Modified** — adapt to `TestMetadata` |

## 5. Key Design Decisions

### Tags as `string_view` with static-duration requirement

Tags are string literals in practice (e.g., `{"fast", "math"}`). Storing
`string_view` avoids heap allocation entirely when tags are present and keeps
`TestMetadata` cheap to copy. The static-duration requirement is the same
convention already used for `suite_name` and `test_name`.

**Trade-off:** Users cannot use dynamically constructed tag strings. This is
acceptable because tags are fixed labels, not computed values.

### Separate `FilterByTag` / `ExcludeByTag` instead of unified filter object

A unified `TestFilter` object would be more composable but more complex. The
current design has three in-place mutation calls (`Filter`, `FilterByTag`,
`ExcludeByTag`) applied in sequence. This is simple, explicit, and sufficient
for the CLI-driven use case where filtering is a one-shot operation.

**Trade-off:** If future features need to reset or reapply filters, in-place
mutation becomes awkward. That scenario is not in requirements; if it arises,
refactoring to a filter-then-view approach is straightforward.

### `TestMetadata` introduced now, not with `#XFAIL`

Introducing `TestMetadata` with `#TAG` rather than deferring it means `#XFAIL`,
`#SKIP`, and `#TMO` each add a single field to an existing struct instead of
restructuring `TestEntry`/`TestResult` again. The `Test<Derived>` builder
(planned for `#XFAIL`) will construct `TestMetadata` and pass it to
`Registry::Add`.

### `TestResult` holds `reference_wrapper<const TestMetadata>`, not a copy

`TestResult` is short-lived (produced and consumed within `RunAll`), and
`TestEntry` outlives it. A reference avoids copying the tags vector. This
matches the architecture overview.

### `--list-verbose` omits brackets for untagged tests

Keeping the output clean: `SuiteName::TestName` with no trailing `[]` for
tests without tags. This makes `--list-verbose` output identical to `--list`
for untagged test suites, reducing visual noise.

### Tag filtering applied after name filtering

This matches the requirements (`--filter` first, tag second) and is efficient:
name filtering narrows the set before tag filtering iterates.

## 6. Feature Changelog

Initial version. No prior `doc/tag-design.md` exists.

**Breaking changes to existing code:**

- `TestEntry` struct layout changes (flat fields replaced by `TestMetadata`)
- `TestResult` struct layout changes (`reference_wrapper` replaces flat fields)
- `Runner::RunTest`, `Runner::PrintResult` must adapt to new field access
- `Suite<Derived>::AddTests` signature gains a trailing parameter
- All existing tests and test code that accesses `TestEntry` or `TestResult`
  fields directly must be updated
