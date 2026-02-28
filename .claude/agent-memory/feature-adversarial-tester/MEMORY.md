# Adversarial Tester Memory

## Architecture Divergence Pattern
- Design docs reference `TestMetadata` as a separate struct, but implementation flattens fields directly into `TestEntry` and `TestResult`.
- This is a systemic issue: `TestEntry` has `suite_name`, `test_name`, `tags` directly; `TestResult` duplicates `suite_name` and `test_name` instead of referencing metadata.
- Will affect every per-test metadata feature (#XFAIL, #SKIP, #TMO).
- `Test<Derived>` builder class referenced in design docs does not exist yet.

## CTest --list Invariant
- Verified: `--list` output is bare names only. No brackets, no metadata.
- `--list-verbose` also bare when no tags assigned at top-level registration.
- Key test: grep for `[` in `--list` output to verify invariant.

## Test Infrastructure Notes
- `clang-tidy.sh` requires compilation database from build dir; running on test files outside build fails with "file not found" for includes. This is a pre-existing issue.
- Coverage script: `./scripts/coverage.sh --agent` provides compact summary.
- All tests are registered in `test/self_test.cpp` main() via namespace `Register()` functions.
- New test files must be added to both `CMakeLists.txt` and `test/self_test.cpp`.

## Edge Cases That Pass Clean
- Empty string tags, whitespace tags, bracket-containing tags: all accepted, no crash.
- Duplicate tags: silently stored multiple times.
- Tag values that look like CLI flags (e.g., `"--fast"`): parsed correctly as tag values.
- Empty registry: FilterByTag, ExcludeByTag, List, ListVerbose all no-op safely.
- Zero tests surviving filters: RunAll returns 0 (all_of on empty range is true).

## Recurring Implementation Patterns
- `std::erase_if` on vector for in-place filtering.
- `std::ranges::find` for linear search in tag vectors.
- MakeArgv helper with const_cast for CLI testing.
