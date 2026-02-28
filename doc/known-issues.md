# Known Issues

## KI-003 — `Test<Derived>` builder with `.Tag()` method does not exist

**Feature**: `#TAG`
**Date discovered**: 2026-02-27
**Status**: Open
**Phase**: Design
**Reproducer**: `doc/tag-design.md` section 4.2

The design document states: "For per-test tags, users use the `Test<Derived>`
builder (from `#XFAIL`) with a `.Tag("x")` method." The `Test<Derived>` builder
class does not exist in the codebase. While `#XFAIL` is `[TODO]`, the design
doc for `#TAG` references it as if it were already available. Per-test tagging
(as opposed to group-level tagging via `AddTests`) is not possible.

## KI-004 — No validation of tag content

**Feature**: `#TAG`
**Date discovered**: 2026-02-27
**Status**: Open
**Phase**: Implementation
**Reproducer**: `test/tag_adversarial_test.cpp` — `TestEmptyStringTag`,
`TestWhitespaceOnlyTag`, `TestTagContainingBrackets`

Empty strings (`""`), whitespace-only strings (`" "`, `"\t"`), and strings
containing bracket characters (`"[tricky]"`) are all accepted as valid tags.
While these do not cause crashes or incorrect filtering behavior, they create
ambiguous `--list-verbose` output. For example, a tag `"[tricky]"` produces
output like `S::A [[tricky], normal]`, which is visually confusing with nested
brackets. The design document does not specify tag content validation rules.

## KI-006 — `--list`/`--list-verbose` interaction is undocumented

**Feature**: `#TAG`
**Date discovered**: 2026-02-27
**Status**: Open
**Phase**: Implementation
**Reproducer**: `test/tag_adversarial_test.cpp` —
`TestListAndListVerboseBothSpecified`,
`TestListVerboseThenListReversedCliOrder`

When both `--list` and `--list-verbose` are specified on the command line,
`--list` always takes precedence regardless of CLI argument order. This is
because the code checks `do_list` before `do_list_verbose`. While this behavior
is reasonable, it is undocumented in the design or requirements. The behavior
should either be documented, or the runner should reject mutually exclusive
flags with an error.

## KI-007 — Architecture diagram stale after `TestMetadata` refactoring

**Feature**: `#TAG`
**Date discovered**: 2026-02-28
**Status**: Open
**Phase**: Architecture
**Reproducer**: `doc/architecture-overview.puml` lines 23, 84

Two entries in the architecture diagram diverge from the implementation after
the KI-001/KI-002 refactoring:

1. `TestMetadata::tags` is shown as `vector<string_view>` (line 84) but the
   implementation uses `std::set<std::string_view>`. The design doc
   (`doc/tag-design.md` section 4.1) correctly specifies `std::set`.
2. `Registry::Add` is shown returning `void` (line 23) but the implementation
   now returns `TestEntry&`, matching the design doc (section 4.5).

The diagram should be updated to reflect the current implementation.
