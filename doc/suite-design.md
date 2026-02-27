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

```cpp
struct TestEntry {
    std::string_view suite_name;
    std::string_view test_name;
    std::function<void()> callable;
};
```

`string_view` is safe because registration always uses string literals (static
storage duration). `callable` encapsulates the full per-test lifecycle (instance
creation, `SetUp`, test method, `TearDown`). The Runner invokes it as a black
box without knowledge of suites.

## 3. `Suite<Derived>`

```cpp
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
```

**CRTP role** — `Derived` is not used inside `Suite` itself. It is consumed by
`Registry::Add<S>`, which constrains `S` to `std::derived_from<S, Suite<S>>`
and uses `S` to construct fresh instances. CRTP makes the derived type available
at the registration call site without macros.

**Virtual `SetUp` / `TearDown`** — Default no-ops. Derived classes override
selectively for fixture setup. Virtual so `override` works for clarity and
compiler checking.

**Protected default constructor** — Prevents standalone `Suite<D>` instantiation.

**Deleted copy/move** — Suite instances are ephemeral: created on the stack
inside the test wrapper lambda, used once, destroyed. Copying or moving
them is always a bug.

**No `Register` on base** — Each derived class defines its own
`static void Register(Registry&)` by convention, not enforced by the base.

### User-facing API

```cpp
class MathSuite : public flul::test::Suite<MathSuite> {
public:
    void TestAddition() { Expect(1 + 1).ToEqual(2); }

    static void Register(flul::test::Registry& r) {
        AddTests(r, "MathSuite", {
            {"TestAddition", &MathSuite::TestAddition},
        });
    }
};
```

With fixtures:

```cpp
class DbSuite : public flul::test::Suite<DbSuite> {
    int connection_id_ = -1;

public:
    void SetUp() override { connection_id_ = 42; }
    void TearDown() override { connection_id_ = -1; }

    void TestQuery() { Expect(connection_id_).ToNotEqual(-1); }

    static void Register(flul::test::Registry& r) {
        r.Add<DbSuite>("DbSuite", "TestQuery", &DbSuite::TestQuery);
    }
};
```

Each test runs on a fresh instance — `SetUp` and `TearDown` bracket every
test, and no state leaks between tests.

## 4. `Registry`

```cpp
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
```

**`Add`** — Creates a lambda that constructs a fresh `S` instance, calls
`SetUp`, invokes the test method, and calls `TearDown`. TearDown runs even
when the test body throws (manual try/catch; `std::scope_exit` is unavailable
in C++23). The lambda captures only the member pointer; names are stored in
`TestEntry` directly.

**`std::default_initializable` constraint** — Makes explicit that `S` must be
default-constructible; prevents cryptic template errors inside the lambda body.

**`Tests`** — Returns a non-owning `span` over `entries_`.

**`Filter`** — Erases entries whose `SuiteName::TestName` does not contain the
given substring. In-place mutation is appropriate for a one-shot CLI tool.

**`List`** — Prints one `SuiteName::TestName` per line for CTest discovery.

## 5. CMake Integration

```cmake
add_library(flul-test INTERFACE)
target_include_directories(flul-test INTERFACE include)
```

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
