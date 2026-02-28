# cpp-implementer Memory

## Project Conventions

### Build
- `just build` — configure (if needed) + compile debug; silent on success
- `just build release` — release variant
- Compilation database at `build/debug/compile_commands.json` (created by `just build`)
- `just lint` — runs clang-tidy via `scripts/clang-tidy.sh -p build/debug` on git-changed files; do not call clang-tidy directly
- `just check` — full quality gate: build + fmt + lint + test in one call
- `just coverage-baseline` / `just coverage-check` — coverage delta workflow (baseline before, check after)

### clang-tidy Pre-existing Warnings (not regressions)
- Include guard naming `FLUL_TEST_*_HPP_` triggers `readability-identifier-naming` — pre-existing, ignored by project
- `bugprone-crtp-constructor-accessibility` on `Suite::Suite()` — pre-existing, ignored

### Test File Conventions
- Test files live in `test/`, follow pattern `<feature>_test.cpp`
- Each test file has a `namespace <feature>_test { void Register(Registry& r); }` at bottom
- Test suite class named `<Feature>Suite : public Suite<<Feature>Suite>`
- Dummy helper classes for test suites go in an anonymous namespace with `NOLINTBEGIN/END(readability-convert-member-functions-to-static)`
- `MakeArgv` helpers go inside anonymous namespace in source files

### CMakeLists.txt
- Add new test files explicitly to `self_test` target in root `CMakeLists.txt`
- Add namespace declaration and `Register()` call to `test/self_test.cpp`

### TestEntry / Registry Pattern
- `TestEntry` has `metadata` (TestMetadata) and `callable`; no direct suite_name/test_name/tags fields
- `TestMetadata` has `suite_name`, `test_name`, `tags` (std::set<std::string_view>), `HasTag()`
- `TestResult` has `std::reference_wrapper<const TestMetadata> metadata` instead of copying names
- `Registry::Add<S>()` returns `TestEntry&` (for builder pattern)
- `Suite<Derived>::AddTests()` out-of-line definition lives in `registry.hpp` (requires Registry to be complete)
- Tags stored as `std::set<std::string_view>` — must point to static-duration storage (string literals)

### clang-tidy Loop Variable Pattern
- `const std::string_view tag` triggers both `misc-const-correctness` (if not const) and `readability-identifier-naming` (kTag if const)
- Solution: use `const auto& tag` in range-for to avoid both diagnostics

### Run() arg parsing pattern (run.hpp)
- Collect all flags in first pass, then apply filters in order: Filter → FilterByTag → ExcludeByTag
- List operations (--list, --list-verbose) happen after filtering but before running
