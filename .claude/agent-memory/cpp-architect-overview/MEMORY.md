# cpp-architect-overview Memory

## Project Namespace / Package Structure
- Single package: `flul::test` — all types live here, no sub-namespaces
- External packages: `User Test Code`, `main()` — shown as `<<external>>`

## Key Architectural Types
- `Registry` — central store; owns `TestEntry` objects; all filtering/shuffle mutate it in-place
- `Runner` — reads from `Registry`, produces `TestResult` per test
- `Run` — free function (shown as class with `<<free function>>` stereotype); parses CLI, orchestrates
- `Suite<Derived>` — CRTP base; user derives from this
- `Test<Derived>` — fluent builder; eager registration (registers on construction, mutates metadata via handle)
- `TestMetadata` — value type; owns test identity + config (names, tags, xfail, skip, timeout)
- `TestEntry` — owns `TestMetadata` + callable; owned by `Registry`
- `TestResult` — references `TestMetadata` via `reference_wrapper`, does not copy

## Confirmed Design Decisions
- All filtering (name, tag, exclude-tag) is in-place mutation on `Registry` — no separate filter object
- Randomization: `Registry::Shuffle(seed)` called in `Run()` after all filters, before runner
- `--list`/`--list-verbose` are never affected by `--randomize`/`--seed`
- `Outcome` enum covers: Pass, Fail, XFail, XPass, Skip, Timeout
- `IsSuccess(Outcome)` is the single source of truth for exit-code contribution

## CLI Flags (all documented in N_cli note on `Run`)
--list, --list-verbose, --filter <pattern>, --tag <tag>, --exclude-tag <tag>, --randomize, --seed <N>, --help

## Execution Order in Run()
1. Parse CLI
2. Filter(pattern) if --filter
3. FilterByTag if --tag
4. ExcludeByTag if --exclude-tag
5. Shuffle(seed) if --randomize or --seed
6. List/ListVerbose (unshuffled — skipped if shuffle already done; listing always uses registration order)
7. Runner(registry).RunAll()

## #RAND Status
- Fully reflected in architecture-overview.puml as of initial setup
- Registry::Shuffle(seed) present; CLI note includes --randomize/--seed <N>

## Design Docs Status (as of 2026-02-27)
- DONE with design doc: #XFAIL (xfail-design.md), #TAG (tag-design.md), #SUITE (suite-design.md), #RUN (run-design.md), #ASSRT (assrt-design.md)
- Has design doc, NEEDS REWRITE: #TMO (tmo-design.md) — old doc describes watchdog/abort(); simplified to post-completion time check (Option A+), no threads
- TODO, no design doc yet: #RAND, #SKIP

## #TMO Confirmed Design (Option A+)
- TestMetadata.timeout: optional<chrono::milliseconds> (duration_cast in builder)
- Outcome::Timeout already in enum; IsSuccess returns false
- Runner::RunTest: after callable completes, compare elapsed to timeout → override to Timeout if exceeded
- No watchdog thread, no jthread, no condition_variable, no abort()
- Registry::ListVerbose: appends [timeout: Xms]
- Summary counts Timeout alongside other outcomes
