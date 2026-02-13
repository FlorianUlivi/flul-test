# flul-test

A C++ experimentation project for exploring language features by implementing a
simple test framework.

## What it does

flul-test is a header-only C++23 test framework with:

- **Assertions** — `Expect(value).ToEqual(...)`, `.ToBeTrue()`, `.ToBeGreaterThan(...)`, etc.
- **Suites** — CRTP base class with `SetUp`/`TearDown` fixture support
- **Runner** — executes tests, captures timing, prints pass/fail diagnostics
- **CTest integration** — per-test discovery via `flul_test_discover()`

## Quick Start

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

## Writing Tests

```cpp
#include <flul/test/expect.hpp>
#include <flul/test/registry.hpp>
#include <flul/test/run.hpp>

using flul::test::Expect;
using flul::test::Registry;
using flul::test::Suite;

class MathSuite : public Suite<MathSuite> {
public:
    void TestAddition() { Expect(1 + 1).ToEqual(2); }

    static void Register(Registry& r) {
        AddTests(r, "MathSuite", {
            {"TestAddition", &MathSuite::TestAddition},
        });
    }
};

auto main(int argc, char* argv[]) -> int {
    Registry registry;
    MathSuite::Register(registry);
    return flul::test::Run(argc, argv, registry);
}
```

## Details

See [CLAUDE.md](CLAUDE.md) for build options, code style, and how to add experiments.
See [doc/](doc/) for architecture overview and component design documents.
