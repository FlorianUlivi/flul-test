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

## 3. Design Goals

- Keep the registration API minimal: tags are the only new parameter
- Unified filtering pipeline in `Registry` (name filter + tag include/exclude)
- Zero overhead when no tags are used
- `--list` output remains bare test names only

## 4. C++ Architecture

### 4.1 `TestMetadata` (modified: `include/flul/test/test_metadata.hpp`)

New field:

```cpp
struct TestMetadata {
    // ...existing fields...
    std::vector<std::string_view> tags;

    auto HasTag(std::string_view tag) const -> bool;
};
```

`tags` stores `string_view` elements pointing to static-duration storage
(string literals), same convention as `suite_name` and `test_name`. The
vector is empty (no heap allocation) when no tags are provided.

`HasTag` returns whether the given tag is present in the vector.

### 4.2 `Suite<Derived>::AddTests` (modified signature)

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

### 4.3 `Registry` (modified: `include/flul/test/registry.hpp`)

New methods and signature changes:

```cpp
// Add gains optional tags parameter:
template <typename S>
    requires std::derived_from<S, Suite<S>> && std::default_initializable<S>
auto Add(std::string_view suite_name, std::string_view test_name,
         void (S::*method)(),
         std::initializer_list<std::string_view> tags = {}) -> TestEntry&;

void FilterByTag(std::span<const std::string_view> include_tags);
void ExcludeByTag(std::span<const std::string_view> exclude_tags);
void ListVerbose() const;
```

**`FilterByTag`** — erases entries that do not carry any of the given tags
(OR semantics). No-op if `include_tags` is empty. Applied after `Filter`.

**`ExcludeByTag`** — erases entries that carry any of the given tags. Applied
after `FilterByTag`. This ordering guarantees exclude-takes-precedence: a test
matching both `--tag` and `--exclude-tag` is included by `FilterByTag` then
removed by `ExcludeByTag`.

**`ListVerbose`** — prints one line per test. Tags are appended in brackets;
tests without tags have no bracket suffix.

### 4.4 `Run()` (modified: `include/flul/test/run.hpp`)

New CLI flags: `--tag <tag>` (repeatable), `--exclude-tag <tag>` (repeatable),
`--list-verbose`.

Execution order in `Run()`:

1. Parse all CLI args
2. `registry.Filter(pattern)` if `--filter`
3. `registry.FilterByTag(include_tags)` if any `--tag` flags
4. `registry.ExcludeByTag(exclude_tags)` if any `--exclude-tag` flags
5. If `--list`: `registry.List()`, return 0
6. If `--list-verbose`: `registry.ListVerbose()`, return 0
7. Otherwise: `Runner(registry).RunAll()`

`--list` and `--list-verbose` respect all filters — they show only the tests
that would run.

### 4.5 `Runner` (modified: `include/flul/test/runner.hpp`)

`RunTest` and `PrintResult` are unchanged in interface. They already operate
on `TestMetadata` (from `#XFAIL`). Tags do not affect test execution or
result reporting — they are purely a filtering concern.

### File Map

| File | Change |
|------|--------|
| `include/flul/test/test_metadata.hpp` | **Modified** — `tags` field + `HasTag` |
| `include/flul/test/suite.hpp` | **Modified** — `AddTests` gains `tags` param |
| `include/flul/test/registry.hpp` | **Modified** — `Add` gains `tags`, `FilterByTag`, `ExcludeByTag`, `ListVerbose` |
| `include/flul/test/run.hpp` | **Modified** — `--tag`, `--exclude-tag`, `--list-verbose` flags |

### Namespace

All types remain in `flul::test`. No new namespaces.

## 5. Key Design Decisions

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

**Breaking changes to existing code:**

- `TestMetadata` gains `tags` field (existing code unaffected unless copying)
- `Suite<Derived>::AddTests` signature gains a trailing parameter (default
  value preserves source compatibility)
- `Registry::Add` signature gains a trailing parameter (default value
  preserves source compatibility)
