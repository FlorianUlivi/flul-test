# Suite & Registry — Detailed Design

## 1. Overview

This document covers the test organisation and registration layer: `Suite<Derived>`,
`TestEntry`, and `Registry`. These components let users define test suites as classes,
register test methods, and provide the Runner with an iterable set of isolated,
self-contained test callables.

### File Map

| File | Purpose |
|---|---|
| `include/flul/test/test_entry.hpp` | `TestEntry` value type |
| `include/flul/test/suite.hpp` | CRTP base class with `SetUp` / `TearDown` lifecycle |
| `include/flul/test/registry.hpp` | Test storage, filtering, and listing |

All files are header-only (templates + `inline`), consistent with the existing
`INTERFACE` library target.

### Dependency Graph

```
registry.hpp ────► test_entry.hpp ──► <functional>
    │                                  <string_view>
    │
    ├──────────────► suite.hpp
    │
    └──────────────► <algorithm>
                     <format>
                     <print>
                     <span>
                     <string>
                     <vector>

suite.hpp ─────────► (no project dependencies)
```

## 2. `TestEntry`

### Interface

```cpp
namespace flul::test {

struct TestEntry {
    std::string_view suite_name;
    std::string_view test_name;
    std::function<void()> callable;
};

}  // namespace flul::test
```

### Design Notes

- `string_view` matches the architecture overview. Registration always uses
  string literals (static storage duration), so views are safe and avoid
  allocation.
- `callable` encapsulates the full per-test lifecycle (instance creation,
  `SetUp`, test method, `TearDown`). The Runner invokes it as a black box
  without knowledge of suites.

## 3. `Suite<Derived>`

### Interface

```cpp
namespace flul::test {

template <typename Derived>
class Suite {
public:
    virtual ~Suite() = default;
    virtual void SetUp() {}
    virtual void TearDown() {}

    Suite(const Suite&) = delete;
    auto operator=(const Suite&) -> Suite& = delete;
    Suite(Suite&&) = delete;
    auto operator=(Suite&&) -> Suite& = delete;

protected:
    Suite() = default;
};

}  // namespace flul::test
```

### Design Decisions

**CRTP role** — The template parameter `Derived` is not used inside `Suite`
itself. Its purpose is consumed by `Registry::Add<S>`, which constrains
`S` to `std::derived_from<S, Suite<S>>` and uses `S` to construct fresh
instances. CRTP makes the derived type available at the registration
call site without macros.

**Virtual `SetUp` / `TearDown`** — Default no-ops. Derived classes override
selectively for fixture setup. These are virtual (not deducing `this`)
because the lifecycle wrapper in `Registry::Add` calls them through the
concrete type directly — virtuality exists only so `override` works for
clarity and compiler checking.

**Protected default constructor** — Prevents standalone `Suite<D>`
instantiation. Only `Derived` (which inherits the constructor) can be
constructed.

**Deleted copy/move** — Suite instances are ephemeral: created on the stack
inside the test wrapper lambda, used once, destroyed. Copying or moving
them is always a bug.

**No `Register` on base** — Each derived class defines its own
`static void Register(Registry&)`. This is a convention enforced by usage,
not by a base-class virtual or CRTP method. Attempting to put `Register`
on the base would require the base to know about `Registry`, creating a
circular dependency.

### User Pattern

```cpp
class MathSuite : public flul::test::Suite<MathSuite> {
public:
    void TestAddition() {
        Expect(1 + 1).ToEqual(2);
    }

    void TestSubtraction() {
        Expect(3 - 1).ToEqual(2);
    }

    static void Register(flul::test::Registry& r) {
        AddTests(r, "MathSuite", {
            {"TestAddition", &MathSuite::TestAddition},
            {"TestSubtraction", &MathSuite::TestSubtraction},
        });
    }
};
```

### With Fixtures

```cpp
class DbSuite : public flul::test::Suite<DbSuite> {
    int connection_id_ = -1;

public:
    void SetUp() override { connection_id_ = 42; }
    void TearDown() override { connection_id_ = -1; }

    void TestQuery() {
        Expect(connection_id_).ToNotEqual(-1);
    }

    static void Register(flul::test::Registry& r) {
        r.Add<DbSuite>("DbSuite", "TestQuery", &DbSuite::TestQuery);
    }
};
```

Each test runs on a fresh `DbSuite` instance — `SetUp` and `TearDown`
bracket every test, and no state leaks between tests.

## 4. `Registry`

### Interface

```cpp
namespace flul::test {

class Registry {
public:
    template <typename S>
        requires std::derived_from<S, Suite<S>> && std::default_initializable<S>
    void Add(std::string_view suite_name, std::string_view test_name,
             void (S::*method)());

    [[nodiscard]] auto Tests() const -> std::span<const TestEntry>;

    void Filter(std::string_view pattern);

    void List() const;

private:
    std::vector<TestEntry> entries_;
};

}  // namespace flul::test
```

### `Add` — Lifecycle Wrapper

```cpp
template <typename S>
    requires std::derived_from<S, Suite<S>> && std::default_initializable<S>
void Registry::Add(std::string_view suite_name, std::string_view test_name,
                   void (S::*method)()) {
    entries_.push_back({
        suite_name,
        test_name,
        [method]() {
            S instance;
            instance.SetUp();
            try {
                (instance.*method)();
            } catch (...) {
                instance.TearDown();
                throw;
            }
            instance.TearDown();
        },
    });
}
```

**Constraint: `std::default_initializable<S>`** — The lambda constructs `S`
via `S instance;`. This concept makes the requirement explicit in the
signature rather than producing a cryptic template error inside the lambda
body.

**TearDown exception safety** — The try/catch ensures `TearDown` runs even
when the test body throws `AssertionError` (or any exception). After
`TearDown`, the original exception re-propagates to the Runner.
`std::scope_exit` is not available in C++23; a manual try/catch is the
simplest alternative. `TearDown` itself should not throw — if it does,
the exception propagates and the original test failure is lost.

**Lambda captures** — Only `method` (the member pointer) is captured.
`suite_name` and `test_name` are stored in `TestEntry` directly, not in
the lambda.

### `Tests`

```cpp
auto Registry::Tests() const -> std::span<const TestEntry> {
    return entries_;
}
```

Returns a non-owning view. The Runner iterates this span to execute tests.

### `Filter`

```cpp
void Registry::Filter(std::string_view pattern) {
    std::erase_if(entries_, [pattern](const TestEntry& e) {
        auto full_name = std::format("{}::{}", e.suite_name, e.test_name);
        return full_name.find(pattern) == std::string::npos;
    });
}
```

- Mutates `entries_` in place — appropriate for a one-shot CLI tool where
  filter is applied once before running.
- Substring match on `"SuiteName::TestName"`.
- An empty pattern retains all tests.

### `List`

```cpp
void Registry::List() const {
    for (const auto& e : entries_) {
        std::println("{}::{}", e.suite_name, e.test_name);
    }
}
```

Outputs one test name per line for CTest discovery (`--list` flag).

## 5. CMake Integration

Add a new `INTERFACE` library target (or extend the existing one):

```cmake
add_library(flul-test INTERFACE)
target_include_directories(flul-test INTERFACE include)
```

The `flul-test-expect` target can remain for existing tests; the new
`flul-test` target encompasses all headers. Eventually the separate
`flul-test-expect` target can be removed.

## 6. Design Rationale Summary

| Decision | Choice | Rationale |
|---|---|---|
| CRTP `Suite<D>` | Type parameter used by `Registry::Add`, not inside `Suite` | Provides derived type for fresh construction without macros |
| `string_view` in `TestEntry` | Views, not owned strings | Names are always string literals (static storage); avoids allocation |
| Virtual `SetUp`/`TearDown` | `virtual`, not deducing `this` | `override` clarity; called on concrete type in practice |
| Protected ctor | `Suite()` is `protected` | Prevents standalone construction of the base template |
| Deleted copy/move | All four special members deleted | Suite instances are single-use, ephemeral |
| `default_initializable` constraint | Explicit on `Add` | Clear error message vs. buried template failure |
| TearDown safety | Manual try/catch in lambda | `std::scope_exit` unavailable; pattern is straightforward |
| In-place `Filter` | `std::erase_if` on the entries vector | One-shot CLI; no need for original + filtered views |
| Header-only | All new files are header-only | Consistent with existing `INTERFACE` library approach |
