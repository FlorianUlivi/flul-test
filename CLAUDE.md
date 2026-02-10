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

### Conversation Style

- Keep feedback very terse
- Prefer structured output over prose

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

