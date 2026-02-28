# Phase 3 Architect Memory

## Project Conventions
- All C++ files are header-only (INTERFACE library target)
- `string_view` used for names/tags — requires static storage duration (string literals)
- NOLINT comments needed for const reference members (cppcoreguidelines-avoid-const-or-ref-data-members)
- `using namespace` in local scope needs NOLINT (google-build-using-namespace)

## Design Patterns
- **TestMetadata**: Central value type for per-test identity + config. Introduced with #XFAIL design
  (was planned for #TAG but #XFAIL is implemented first). Fields: suite_name, test_name, xfail.
  Future features add fields: tags (#TAG), skip (#SKIP), timeout (#TMO).
- **Outcome enum**: Replaces `bool passed` in TestResult. Values: Pass, Fail, XFail, XPass.
  Extended by #SKIP (Skip) and #TMO (Timeout). `IsSuccess()` free function for exit-code logic.
- **Test<Derived> builder**: Eager registration pattern — constructor calls Registry::Add, stores
  `TestEntry&`, builder methods mutate metadata in place. No deferred commit, no destructor logic.
  Reference safe because builder is temporary within Register().
- **Registry::Add returns TestEntry&**: Enables builder pattern. Vector reallocation safety:
  each builder chain completes before next Add call.
- **TestResult references TestMetadata**: Uses `reference_wrapper<const TestMetadata>` to avoid copying.
  Safe because Registry outlives Runner::RunAll().
- **In-place filtering pipeline**: Filter -> FilterByTag -> ExcludeByTag, applied sequentially in Run().
- **AddTests unchanged for common case**: Bulk registration without builder still works.
  Per-test metadata requires direct Test<Derived> construction.

## Existing Design Docs
- `doc/suite-design.md` — Suite, TestEntry, Registry (current impl matches)
- `doc/run-design.md` — Runner, TestResult, Run(), CTest integration
- `doc/assrt-design.md` — Assertions
- `doc/tag-design.md` — Tags (#TAG) — introduces TestMetadata, modifies TestEntry/TestResult/Registry/Run
- `doc/xfail-design.md` — Expected Failure (#XFAIL) — introduces TestMetadata, Outcome, Test<Derived> builder
- `doc/rand-design.md` — Randomized order (#RAND) — Registry::Shuffle, --randomize/--seed CLI, two-level shuffle
- `doc/tmo-design.md` — Timeout (#TMO) — post-test clock check, Outcome::Timeout

## #TMO Design Notes (v2 — Option A+)
- Post-test clock check: after try/catch, compare elapsed vs metadata.timeout; override to Timeout if exceeded
- Zero multithreading — no jthread, no abort(), no watchdog, no condition variables
- Hung tests block forever (accepted trade-off; CTest handles process-level timeouts)
- Outcome::Timeout is reachable in normal execution (unlike v1 where it was forward-compat only)
- Timeout overrides ALL base outcomes (Pass/Fail/XFail/XPass -> Timeout)
- Stored as std::optional<std::chrono::milliseconds> in TestMetadata; builder template accepts any duration
- Skip takes precedence over timeout (checked first in RunTest)
- No global default timeout, no CLI override — registration-time only

## #RAND Design Notes
- Two-level shuffle: group by suite_name, shuffle groups, shuffle within groups, flatten
- Single std::mt19937 seeded once for full determinism
- Seed validation: std::from_chars into uint64_t, then range-check for uint32_t
- --seed implicitly enables randomization (no need for --randomize alongside)
- Shuffle call site: after all filtering, before Runner, after --list/--list-verbose exit points
