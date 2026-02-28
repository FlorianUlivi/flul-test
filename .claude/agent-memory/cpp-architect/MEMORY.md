# flul-test Architecture Memory

## Project Structure
- Header-only library under `include/flul/test/`
- No `.cpp` source files; all code is templates + `inline`
- CMake `INTERFACE` library target
- Namespace: `flul::test`

## Key Files
- `suite.hpp` — CRTP base, forward-declares `Registry`
- `registry.hpp` — stores `TestEntry` vector, defines `Suite::AddTests` out-of-line
- `runner.hpp` — iterates tests, catches exceptions, prints results
- `run.hpp` — CLI parsing, wires Registry to Runner
- `test_entry.hpp` — aggregate: suite_name, test_name, callable
- `expect.hpp` — value assertions with deducing `this`
- `expect_callable.hpp` — callable/exception assertions
- `assertion_error.hpp` — exception with actual/expected/location
- `stringify.hpp` — concept-constrained stringification + Demangle

## Established Patterns
- `Suite::AddTests()` defined in `registry.hpp` (breaks circular dep)
- `TestEntry` uses `string_view` (names are always string literals)
- `Registry::Filter()` mutates `entries_` in-place via `std::erase_if`
- Runner catches `AssertionError`, `std::exception`, `...` separately
- All assertion methods use deducing `this` for lvalue/rvalue chaining
- Manual CLI parsing in `Run()` — no library

## Architecture Decisions (confirmed)
- Tags go on `TestEntry` as `vector<string_view>` (passed at registration)
- `Suite::AddTests` needs a tags-aware overload
- `Registry` gets `FilterByTag`, `ExcludeByTag`, `Shuffle` methods
- `Run()` gets `--tag`, `--exclude-tag`, `--randomize`, `--seed` flags
- `ToBeNear` is a new method on `Expect<T>` constrained on `std::floating_point`
- Execution order: name filter -> tag filter -> randomize -> run
