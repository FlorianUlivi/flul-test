# Adversarial Tester Memory

## Architecture Divergence Pattern
- KI-001/KI-002 RESOLVED: `TestMetadata` now exists as separate struct in `include/flul/test/test_metadata.hpp`.
- `TestEntry` contains `TestMetadata metadata` by value; `TestResult` holds `reference_wrapper<const TestMetadata>`.
- `Registry::Add` returns `TestEntry&` (was `void`).
- Architecture diagram (`architecture-overview.puml`) still stale: shows `vector<string_view>` for tags (should be `set`), shows `Add` returning `void` (should be `TestEntry&`). Filed as KI-007.
- `Test<Derived>` builder class referenced in design docs does not exist yet (KI-003 still open).
- `Registry::Add` return ref safety: by convention only (builder is temporary). Holding ref across Add calls is UB after reallocation.

## CTest --list Invariant
- Verified: `--list` output is bare names only. No brackets, no metadata.
- `--list-verbose` also bare when no tags assigned at top-level registration.
- Key test: grep for `[` in `--list` output to verify invariant.

## Test Infrastructure Notes
- `just build` — configure + compile debug; silent on success
- `just test` — run all tests; shows only failures + count on success
- `just check` — full quality gate: build + fmt + lint + test
- `just coverage-baseline` / `just coverage-check` — coverage delta workflow
- `just lint` wraps `scripts/clang-tidy.sh -p build/debug`; do not call clang-tidy directly
- All tests are registered in `test/self_test.cpp` main() via namespace `Register()` functions.
- New test files must be added to both `CMakeLists.txt` and `test/self_test.cpp`.

## Edge Cases That Pass Clean
- Empty string tags, whitespace tags, bracket-containing tags: all accepted, no crash.
- Duplicate tags: deduplicated at registration via `std::set`; warning emitted to stderr per dup (KI-005 fixed).
- Tag values that look like CLI flags (e.g., `"--fast"`): parsed correctly as tag values.
- Empty registry: FilterByTag, ExcludeByTag, List, ListVerbose all no-op safely.
- Zero tests surviving filters: RunAll returns 0 (all_of on empty range is true).

## Recurring Implementation Patterns
- `std::erase_if` on vector for in-place filtering.
- `std::ranges::find` for linear search in tag vectors.
- MakeArgv helper with const_cast for CLI testing.

## stdout Capture Pitfall
- `std::println(...)` (to stdout) writes via `FILE*`, bypasses `std::cout.rdbuf()`.
- `std::println(std::cerr, ...)` DOES go through `std::cerr.rdbuf()` -- can be captured.
- For stdout capture tests, verify data model directly instead of capturing output.
