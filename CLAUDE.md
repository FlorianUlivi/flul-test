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

**REQUIRED**: Run these tools whenever code is changed or added:
- `clang-format -i <file>` - Format code before committing
- `./scripts/clang-tidy.sh <file>` - Static analysis and naming validation

### Coverage

**REQUIRED**: Check coverage when adding or changing code:

1. **Before** changes: `./scripts/coverage.sh --agent` — record TOTAL line% and branch%
2. Make changes
3. **After** changes: `./scripts/coverage.sh --agent` — compare TOTAL line% and branch%
4. If coverage decreased >1%: examine "Uncovered Lines" output, add tests, re-run
5. If after one iteration decrease is within 1%: acceptable, proceed
6. If still >1% decrease: ask user for confirmation before proceeding

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
cmake --preset debug
cmake --build --preset debug
```

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

Process for adding features:

1. Update the requirements
2. Update the architecture overview
3. Add detailed architecture and design decision documentation

When creating or updating architecture diagrams:

- Use clang-uml style for documenting C++ in PlantUML
- Focus on public interfaces and key relationships
- Use packages to organize by namespace
- Commit `.puml` files to version control

