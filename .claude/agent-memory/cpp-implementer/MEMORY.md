# cpp-implementer Memory

## Project Conventions

### Build
- `cmake --build --preset debug` — standard build command
- Compilation database at `build/debug/compile_commands.json`
- clang-tidy must be run with `-p build/debug` to use the database: `./scripts/clang-tidy.sh -p build/debug <file>`
- Without compilation database, clang-tidy gives false positives (missing headers, unknown names)

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
- `TestEntry` is the data struct (suite_name, test_name, callable, tags)
- `Registry::Add<S>()` is the template registration method
- `Suite<Derived>::AddTests()` out-of-line definition lives in `registry.hpp` (requires Registry to be complete)
- Tags stored as `std::vector<std::string_view>` — must point to static-duration storage (string literals)

### Run() arg parsing pattern (run.hpp)
- Collect all flags in first pass, then apply filters in order: Filter → FilterByTag → ExcludeByTag
- List operations (--list, --list-verbose) happen after filtering but before running
