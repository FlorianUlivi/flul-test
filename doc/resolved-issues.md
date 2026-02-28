# Resolved Issues

Issues moved here once fixed. See `doc/known-issues.md` for open issues.

## KI-005 — Duplicate tags silently accepted

**Feature**: `#TAG`
**Date discovered**: 2026-02-27
**Status**: Resolved
**Phase**: Implementation
**Reproducer**: `test/tag_adversarial_test.cpp` — `TestDuplicateTagsOnSameTest`

Registering the same tag multiple times on a single test (e.g.,
`{"fast", "fast", "fast"}`) stored three entries, wasting memory and producing
redundant `--list-verbose` output.

**Resolved**: 2026-02-28
**Fix**: Changed `TestEntry::tags` from `std::vector<std::string_view>` to
`std::set<std::string_view>`. `Registry::Add` now deduplicates at registration
time via `set::insert`, emitting a `[flul-test] warning:` to `std::cerr` for
each duplicate. `HasTag` simplified to `tags.contains(tag)`.

## KI-001 — `TestMetadata` struct does not exist in implementation

**Feature**: `#TAG`
**Date discovered**: 2026-02-27
**Status**: Resolved
**Phase**: Design
**Reproducer**: `doc/tag-design.md` section 4.1 vs `include/flul/test/test_entry.hpp`

The design document specified that `tags` and `HasTag()` should live on a
`TestMetadata` struct, with `TestEntry` containing `TestMetadata` and
`TestResult` referencing it via `std::reference_wrapper<const TestMetadata>`.
The implementation had no `TestMetadata` struct; fields lived directly on
`TestEntry` and `TestResult` duplicated name fields.

**Resolved**: 2026-02-28
**Fix**: New `include/flul/test/test_metadata.hpp` with `TestMetadata` struct
containing `suite_name`, `test_name`, `tags` (`std::set<std::string_view>`),
and `HasTag()`. `TestEntry` now holds `TestMetadata metadata` by value.
`TestResult` holds `std::reference_wrapper<const TestMetadata>`.

## KI-002 — `Registry::Add` return type diverges from design

**Feature**: `#TAG`
**Date discovered**: 2026-02-27
**Status**: Resolved
**Phase**: Design
**Reproducer**: `doc/tag-design.md` section 4.3 vs `include/flul/test/registry.hpp`

The design document specified `Registry::Add` should return `TestEntry&` to
support the `Test<Derived>` builder pattern. The implementation returned `void`.

**Resolved**: 2026-02-28
**Fix**: `Registry::Add` now returns `TestEntry&` (reference to the
just-appended entry in the internal vector), enabling the builder pattern for
future features (#XFAIL, #SKIP, #TMO).
