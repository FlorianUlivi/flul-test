# Requirements Writer Memory

## File Location
`doc/requirements.md` — sole file to edit.

## Structure & Numbering
- Top-level sections: `## Goal`, `## Scope`
- Under Scope: `### Features`, `### Design Decisions` (bullets), `### C++23 Features to Exploit` (table)
- `### Features` is divided into `####` group subsections; each group has its own numbered list starting from 1
- Sub-bullets use `-` at 3-space indent
- Status markers inline after bold name: `**Name** \`[DONE]\`` or `**Name** \`[TODO]\``

## Features Section Groups (in order)
| Group | Items |
|---|---|
| `#### Test Structure` | Test Registration, Test Suites, Fixtures |
| `#### Test Execution Control` | Test Runner, Test Filtering, Randomized Execution Order |
| `#### Per-Test Metadata` | Tags, Expected Failure (xfail), Skip, Timeout |
| `#### Assertions` | Assertions |
| `#### Reporting & Output` | Output |
| `#### Tooling & Integration` | CTest Integration, Coverage |

## Per-Test Metadata Pattern
Consistent structure for registration-time, per-test attributes:
1. Set via fluent `Test<Derived>` builder method (`.ExpectFail()`, `.Skip()`, `.Timeout(duration)`)
2. Orthogonal to tags/filtering — applied after filtering
3. `--list` output shows a marker (e.g., `[xfail]`, `[skip]`, `[timeout: Xms]`)
4. Cannot be toggled via CLI flags
5. Summary line includes count alongside pass/fail/skip/xfail/xpass/timeout

## Key Terminology
- "shall" = mandatory; "should" = recommendation
- Suite, test, runner, registry, fixture, tag, xfail, xpass, skip, timeout
- `TestMetadata` holds identity + config (names, tags, flags like xfail/timeout)
- `TestEntry` owns `TestMetadata`; `TestResult` borrows via `std::reference_wrapper<const TestMetadata>`
- Builder: `Test<Derived>` fluent builder for registration-time config

## Design Decisions Section
`TestMetadata` already mentions "xfail or timeout" — timeout was anticipated in the design decisions before item 14 was written.
