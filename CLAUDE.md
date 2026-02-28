# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with
code in this repository.

## Workflow Directives

### When to Use Plan Mode

Use plan mode when:
- Implementing new features that involve design decisions
- Making architectural changes
- Modifying multiple components that require coordination

### Code Quality Automation

**REQUIRED**: Run after any code change:
- `just check` — format + lint + build + test; silent on success, full diagnostics on failure

### Coverage

**REQUIRED**: Check coverage when adding or changing code:

1. **Before** changes: `just coverage-baseline` — snapshot current totals
2. Make changes
3. **After** changes: `just check && just coverage-check`
4. If `coverage-check` reports regression >1%: examine output, add tests, re-run
5. If after one iteration decrease is within 1%: acceptable, proceed
6. If still >1% decrease: ask user for confirmation before proceeding

### Feature Development Workflow

All feature work follows a 5-phase pipeline. **User clearance is required between every phase.**

| Phase | Agent | Scope |
|-------|-------|-------|
| 1 — Requirements | `requirements-writer` | Define and confirm requirements in `doc/requirements.md` |
| 2 — Architecture | `cpp-architect-overview` | Update `doc/architecture-overview.puml`; flag features needing design and any refactoring |
| 3 — Detailed Design | `cpp-architect-design` | One feature per invocation; produces `doc/<feature>-design.md` |
| 4 — Implementation | `cpp-implementer` | One feature per invocation; writes tests, implements code, runs checks |
| 5 — Adversarial Testing | `feature-adversarial-tester` | Optional; required when `src/` or `include/` changed in Phase 4 |

**Orchestration rules:**
1. Always start with Phase 1, even if only confirming existing requirements are complete
2. After each phase completes, present output to the user and **explicitly ask for clearance** before proceeding
3. Phases 3 and 4 are per-feature — repeat with user clearance between each feature
4. If Phase 2 or 3 flags refactoring: pause, surface it to the user as a named step, implement it as a separate `feature/refactor-<desc>` branch with its own user clearance, then resume the original phase
5. Phase 5 is optional but required before closing if Phase 4 touched `src/` or `include/`

### Conversation Style

- Keep feedback very terse
- Prefer structured output over prose

## Git Strategy

### Branches

| Branch | Purpose |
|--------|---------|
| `main` | Always stable; tagged releases only |
| `develop` | Active development; base for all work |
| `feature/<name>` | One feature or fix per branch; branches from `develop` |

### Workflow

**Feature development:**
1. `git checkout -b feature/<name> develop`
2. Implement; keep branch short-lived
3. Merge back into `develop` when done; delete branch

Use a feature branch for any change that warrants plan mode.

**Release:**
1. Verify `develop` is stable and tests pass
2. Bump version, commit to `develop`
3. `git checkout main && git merge --no-ff develop`
4. `git tag -a vX.Y.Z -m "Release X.Y.Z"`
5. `git checkout develop && git merge main` ← back-merge to stay in sync

**Hotfix (bug in released `main`):**
1. `git checkout -b feature/hotfix-<desc> main`
2. Fix and test
3. Merge into `main`, tag new patch version
4. Merge into `develop`

### Versioning

Semantic versioning: `MAJOR.MINOR.PATCH`
- `PATCH` — bug fixes
- `MINOR` — new backwards-compatible features
- `MAJOR` — breaking changes

## Project Structure

```
flul-test
├── build   - build output
├── cmake   - cmake includes
├── doc     - documentation
├── include - public headers
├── src     - source files and private headers
└── test    - unit tests (custom framework, to be bootstrapped)
```

## Build

```bash
just build          # configure (if needed) + compile debug; silent on success
just build release  # release variant
```

The `justfile` at the repo root is the primary task runner. Run `just --list` to see all available recipes. Requires `just` (`brew install just`).

## Code Style

This project follows the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html).

### Naming Conventions

- **Types** (classes, structs, enums, type aliases): `CamelCase`
- **Functions/Methods**: `CamelCase`
- **Variables**: `snake_case`
- **Class members**: `snake_case_` (trailing underscore)
- **Constants**: `kCamelCase` (leading k)
- **Macros**: `SCREAMING_SNAKE_CASE`
- **Namespaces**: `snake_case`

### Header Guards

Use include guards (not `#pragma once`), following the Google C++ Style Guide.
Guard name is derived from the include-relative path, all uppercase, with `/`
and `.` replaced by `_`, and a trailing `_`:

```
flul/test/foo_bar.hpp  →  FLUL_TEST_FOO_BAR_HPP_
```

```cpp
#ifndef FLUL_TEST_FOO_BAR_HPP_
#define FLUL_TEST_FOO_BAR_HPP_

// ... content ...

#endif  // FLUL_TEST_FOO_BAR_HPP_
```

### Project Overrides

The following deviate from pure Google style (see `.clang-format`):

- Indent width: 4 spaces (Google uses 2)
- Column limit: 100 (Google uses 80)
- Pointer alignment: Left (Google uses Right)
- File names: `snake_case.hpp` for headers, `snake_case.cpp` for source files (Google uses `.h`/`.cc`)

### Comments

- Only use comments to explain why something is done, if it is not obvious,
  rather than what is done. The code should explain what is happening.

## CMake Guidelines

- **Always list source files explicitly** in CMakeLists.txt - never use file globbing/discovery

## Documentation

Documentation lives in `doc/`.

### Requirements and Architecture

Always keep `doc/requirements.md` and `doc/architecture-overview.puml` up to
date when adding or updating the requirements.

Each feature in `doc/requirements.md` must carry a status marker:
- `[DONE]` — fully implemented and merged
- `[TODO]` — planned but not yet implemented

Each feature also carries a stable short identifier slug of the form `` `#SLUG` ``
(max 5 uppercase ASCII letters), e.g. `` `#REG` ``, `` `#XFAIL` ``. The slug is
assigned once and never changes, regardless of renumbering or regrouping.

Use the slug consistently across all artefacts that relate to a feature:
- **Documents**: reference features as `#SLUG` in design docs and architecture notes
- **File names**: design docs are named `doc/<slug-lowercase>-design.md` (e.g. `doc/xfail-design.md`)
- **Test names**: test suite or test method names include the slug in lowercase (e.g. `XfailTest`, `SkipTest`)
- **Commit messages**: prefix the subject with the slug in lowercase, e.g. `xfail: add ExpectFail builder method`

Process for adding features:

1. Add the feature to `doc/requirements.md` with `[TODO]` and a new unique `#SLUG`
2. Update the architecture overview
3. Add detailed architecture and design decision documentation

When completing a feature and merging back into `develop`:

1. Flip the marker from `[TODO]` to `[DONE]` in `doc/requirements.md`
2. Verify no other markers need updating

When creating or updating architecture diagrams:

- Use clang-uml style for documenting C++ in PlantUML
- Focus on public interfaces and key relationships
- Use packages to organize by namespace
- Commit `.puml` files to version control

## Known Issues

Open issues live in `doc/known-issues.md`. Resolved issues are in `doc/resolved-issues.md`.

### Format

```markdown
## KI-NNN — <short title>

**Feature**: `#SLUG`
**Date discovered**: YYYY-MM-DD
**Status**: Open | In Progress | Wontfix | Deferred
**Phase**: Requirements | Architecture | Design | Implementation
**Reproducer**: `<test name or file>`

<concise description: what was expected, what actually happened>
```

### ID Scheme

`KI-NNN` — sequential, never reused. Use IDs when referencing issues in chat,
commits, and PRs (e.g. `Fixes KI-004`).

### Phase Definitions

- **Requirements**: requirement is missing, contradictory, or underspecified
- **Architecture**: violates an architectural constraint or is fundamentally misfit
- **Design**: `<slug>-design.md` is incorrect, incomplete, or misleads the implementer
- **Implementation**: code does not correctly implement the design or requirements

### When to Document

| Situation | Action |
|-----------|--------|
| Clearly trivial (typo, status marker flip, one-liner) | Fix immediately, no entry |
| Clearly non-trivial (blocked, systemic, needs decision, regression risk) | Add entry to `doc/known-issues.md` |
| Uncertain | Ask the user |

### Status Lifecycle

| Status | Meaning |
|--------|---------|
| `Open` | Confirmed, no fix in progress |
| `In Progress` | Fix actively being worked; reference branch/PR |
| `Wontfix` | Acknowledged, intentionally not fixed; state reason |
| `Deferred` | Will be addressed with a future feature; reference `#SLUG` |

### Addressing Issues via Prompt

`KI-NNN` in a prompt means: address that known issue.

1. Read the issue entry to determine phase and feature slug
2. Route to the Phase N agent per the Feature Development Workflow table. Chain phases if the fix spans multiple. Phase 5 (`feature-adversarial-tester`) is required before closing if any code in `src/` or `include/` was changed.
3. On close: move entry from `doc/known-issues.md` to `doc/resolved-issues.md`;
   add `**Resolved**: YYYY-MM-DD` and `**Fix**: <commit or PR>`

