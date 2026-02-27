# Known Issues

## [REQUIREMENTS] #TAG -- Status marker not flipped to [DONE]

**Date discovered**: 2026-02-27
**Status**: Open
**Level**: Requirements
**Reproducer**: `doc/requirements.md` line 59

The `#TAG` feature has been fully implemented and merged to `develop` (commit
`9dd6043`), but `doc/requirements.md` still marks it as `[TODO]`. Per the
project workflow, the marker should be flipped to `[DONE]` when the feature is
merged back into `develop`.

## [DESIGN] #TAG -- TestMetadata struct does not exist in implementation

**Date discovered**: 2026-02-27
**Status**: Open
**Level**: Design
**Reproducer**: `doc/tag-design.md` section 4.1 vs `include/flul/test/test_entry.hpp`

The design document specifies that `tags` and `HasTag()` should live on a
`TestMetadata` struct (section 4.1), with `TestEntry` containing `TestMetadata`
and `TestResult` referencing it via `std::reference_wrapper<const TestMetadata>`.
The architecture diagram (`doc/architecture-overview.puml`) also shows
`TestMetadata` as a separate struct.

In the actual implementation, there is no `TestMetadata` struct. The `tags`
field and `HasTag` method live directly on `TestEntry`. `TestResult` duplicates
`suite_name` and `test_name` as separate fields instead of referencing
`TestMetadata`. This diverges from the documented architecture and will become
a maintenance burden as more per-test metadata is added (#XFAIL, #SKIP, #TMO).

## [DESIGN] #TAG -- Registry::Add return type diverges from design

**Date discovered**: 2026-02-27
**Status**: Open
**Level**: Design
**Reproducer**: `doc/tag-design.md` section 4.3 vs `include/flul/test/registry.hpp` line 23

The design document specifies `Registry::Add` should return `TestEntry&` to
support the `Test<Derived>` builder pattern. The implementation returns `void`.
This will block the `#XFAIL` feature which needs `Add` to return a reference
for builder chaining (e.g., `.ExpectFail()`).

## [DESIGN] #TAG -- Test<Derived> builder with .Tag() method does not exist

**Date discovered**: 2026-02-27
**Status**: Open
**Level**: Design
**Reproducer**: `doc/tag-design.md` section 4.2

The design document states: "For per-test tags, users use the `Test<Derived>`
builder (from `#XFAIL`) with a `.Tag("x")` method." The `Test<Derived>` builder
class does not exist in the codebase. While `#XFAIL` is `[TODO]`, the design
doc for `#TAG` references it as if it were already available. Per-test tagging
(as opposed to group-level tagging via `AddTests`) is not possible.

## [IMPLEMENTATION] #TAG -- No validation of tag content

**Date discovered**: 2026-02-27
**Status**: Open
**Level**: Implementation
**Reproducer**: `test/tag_adversarial_test.cpp` — `TestEmptyStringTag`,
`TestWhitespaceOnlyTag`, `TestTagContainingBrackets`

Empty strings (`""`), whitespace-only strings (`" "`, `"\t"`), and strings
containing bracket characters (`"[tricky]"`) are all accepted as valid tags.
While these do not cause crashes or incorrect filtering behavior, they create
ambiguous `--list-verbose` output. For example, a tag `"[tricky]"` produces
output like `S::A [[tricky], normal]`, which is visually confusing with nested
brackets. The design document does not specify tag content validation rules.

## [IMPLEMENTATION] #TAG -- Duplicate tags are silently accepted

**Date discovered**: 2026-02-27
**Status**: Open
**Level**: Implementation
**Reproducer**: `test/tag_adversarial_test.cpp` — `TestDuplicateTagsOnSameTest`

Registering the same tag multiple times on a single test (e.g.,
`{"fast", "fast", "fast"}`) stores three entries. While this does not break
filtering (HasTag still finds it), it wastes memory and produces redundant
`--list-verbose` output like `S::A [fast, fast, fast]`. The design document
does not address deduplication.

## [IMPLEMENTATION] #TAG -- --list and --list-verbose interaction is undocumented

**Date discovered**: 2026-02-27
**Status**: Open
**Level**: Implementation
**Reproducer**: `test/tag_adversarial_test.cpp` —
`TestListAndListVerboseBothSpecified`,
`TestListVerboseThenListReversedCliOrder`

When both `--list` and `--list-verbose` are specified on the command line,
`--list` always takes precedence regardless of CLI argument order. This is
because the code checks `do_list` before `do_list_verbose`. While this behavior
is reasonable, it is undocumented in the design or requirements. The behavior
should either be documented, or the runner should reject mutually exclusive
flags with an error.
