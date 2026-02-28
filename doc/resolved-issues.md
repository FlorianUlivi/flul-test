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
