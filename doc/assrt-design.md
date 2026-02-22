# Expect Classes — Detailed Design

## 1. Overview

This document covers the assertion subsystem of flul-test: `Expect<T>`,
`ExpectCallable<F>`, `AssertionError`, and `Stringify`. These components let
tests express expectations on values and callables, producing clear diagnostics
on failure.

### File Map

| File | Purpose |
|---|---|
| `include/flul/test/stringify.hpp` | Concept-constrained stringification + `Demangle` |
| `include/flul/test/assertion_error.hpp` | Exception class (`std::exception` subclass) |
| `include/flul/test/expect.hpp` | Value assertion template with chaining |
| `include/flul/test/expect_callable.hpp` | Callable/exception assertion template |

All files are header-only (templates + `inline`). The CMake target is an
`INTERFACE` library.

### Dependency Graph

```
expect.hpp ──────────► assertion_error.hpp ──► <exception>
    │                        │                 <source_location>
    │                        │                 <string>
    │                        ▼
    └───────────────► stringify.hpp ──────────► <format>
                                                <concepts>
                                                <sstream>
expect_callable.hpp ► assertion_error.hpp
    │                        │
    │                        ▼
    └──────────────► stringify.hpp (Demangle only)
```

## 2. `AssertionError`

### Interface

```cpp
namespace flul::test {

class AssertionError : public std::exception {
public:
    std::string actual;
    std::string expected;
    std::source_location location;

    constexpr AssertionError(std::string actual, std::string expected,
                             std::source_location location);

    auto what() const noexcept -> const char* override;

private:
    std::string what_;
};

}  // namespace flul::test
```

### Constructor Logic

The constructor initializes all three public fields via move, then builds
`what_` eagerly:

```
{location.file_name()}:{location.line()}: assertion failed
  expected: {expected}
    actual: {actual}
```

The `{file}:{line}:` prefix is IDE-clickable (Clion, VS Code, Xcode).

### Design Notes

- Public data members allow the Runner and output layer to access fields
  directly without getters, matching the architecture diagram.
- `what_` is built once in the constructor. `what()` returns
  `what_.c_str()`.
- The class is not a template — `actual` and `expected` are always
  pre-stringified by the caller (`Expect` / `ExpectCallable`).

## 3. `Stringify`

### Overload Set

Three free-function overloads, selected by concept constraints. Overload
resolution prefers the most constrained match.

```cpp
namespace flul::test {

// (1) Primary: types with std::format support
template <typename T>
    requires std::formattable<T, char>
auto Stringify(const T& value) -> std::string;

// (2) Fallback: types with operator<<
template <typename T>
    requires (!std::formattable<T, char>) &&
             requires(std::ostream& os, const T& v) { os << v; }
auto Stringify(const T& value) -> std::string;

// (3) Final fallback: non-printable types
template <typename T>
    requires (!std::formattable<T, char>) &&
             (!requires(std::ostream& os, const T& v) { os << v; })
auto Stringify(const T& value) -> std::string;
// Returns "<non-printable>"

}  // namespace flul::test
```

### `Demangle`

```cpp
namespace flul::test {

auto Demangle(const char* mangled) -> std::string;

}  // namespace flul::test
```

Wraps `__cxa_demangle` (available on macOS via `<cxxabi.h>`). Used by
`ExpectCallable` to produce readable type names in error messages.

### Implementation Notes

- Overload (1) uses `std::format("{}", value)`.
- Overload (2) uses `std::ostringstream`.
- Overload (3) returns the literal string `"<non-printable>"`.
- The three overloads are mutually exclusive via negated constraints — no
  ambiguity.

## 4. `Expect<T>`

### Interface

```cpp
namespace flul::test {

template <typename T>
class Expect {
public:
    constexpr Expect(T actual,
                     std::source_location loc = std::source_location::current());

    constexpr auto ToEqual(this auto&& self, const T& expected) -> decltype(auto)
        requires std::equality_comparable<T>;

    constexpr auto ToNotEqual(this auto&& self, const T& unexpected) -> decltype(auto)
        requires std::equality_comparable<T>;

    constexpr auto ToBeTrue(this auto&& self) -> decltype(auto)
        requires std::convertible_to<T, bool>;

    constexpr auto ToBeFalse(this auto&& self) -> decltype(auto)
        requires std::convertible_to<T, bool>;

    constexpr auto ToBeGreaterThan(this auto&& self, const T& bound) -> decltype(auto)
        requires std::totally_ordered<T>;

    constexpr auto ToBeLessThan(this auto&& self, const T& bound) -> decltype(auto)
        requires std::totally_ordered<T>;

private:
    T actual_;
    std::source_location loc_;
};

}  // namespace flul::test
```

### Design Decisions

**Deducing `this`** — `auto Method(this auto&& self, ...) -> decltype(auto)`
handles both lvalue and rvalue chains:

```cpp
// lvalue chain
auto e = Expect(x);
e.ToBeGreaterThan(0).ToBeLessThan(100);

// rvalue chain (temporary)
Expect(x).ToBeGreaterThan(0).ToBeLessThan(100);
```

The return is `std::forward<decltype(self)>(self)`, preserving value category.

**Value storage** — `T actual_` is stored by value (moved in from the
constructor). This avoids dangling references when `Expect` is constructed
with a temporary.

**Per-method constraints** — Each method constrains only what it needs.
`Expect<T>` itself is unconstrained, so it can be instantiated for any `T`;
methods that don't apply simply don't participate in overload resolution.

**constexpr** — Constructor and all methods are marked `constexpr`. If an
assertion fails at compile time, the `throw` expression makes the
evaluation non-constant — the compiler emits an error at the call site.
This gives compile-time assertion checking for free.

**CTAD** — `Expect(value)` deduces `T` via implicit class template argument
deduction. No explicit deduction guide is needed.

### Error Messages per Method

| Method | Condition | `expected` | `actual` |
|---|---|---|---|
| `ToEqual(v)` | `actual_ != v` | `Stringify(v)` | `Stringify(actual_)` |
| `ToNotEqual(v)` | `actual_ == v` | `"not " + Stringify(v)` | `Stringify(actual_)` |
| `ToBeTrue()` | `!actual_` | `"true"` | `Stringify(actual_)` |
| `ToBeFalse()` | `actual_` | `"false"` | `Stringify(actual_)` |
| `ToBeGreaterThan(v)` | `actual_ <= v` | `"greater than " + Stringify(v)` | `Stringify(actual_)` |
| `ToBeLessThan(v)` | `actual_ >= v` | `"less than " + Stringify(v)` | `Stringify(actual_)` |

All methods throw `AssertionError{actual_str, expected_str, loc_}` on
failure.

## 5. `ExpectCallable<F>`

### Interface

```cpp
namespace flul::test {

template <std::invocable F>
class ExpectCallable {
public:
    constexpr ExpectCallable(F callable,
                             std::source_location loc = std::source_location::current());

    template <typename E>
    auto ToThrow() -> void;

    auto ToNotThrow() -> void;

private:
    F callable_;
    std::source_location loc_;
};

}  // namespace flul::test
```

### `ToThrow<E>()` Logic

1. Invoke `callable_()` inside a try-catch.
2. Catch `E&` — assertion passes; return.
3. Catch `...` — assertion fails: expected `Demangle(typeid(E).name())`,
   actual `"different exception"`.
4. No exception thrown — assertion fails: expected
   `Demangle(typeid(E).name())`, actual `"no exception"`.

### `ToNotThrow()` Logic

1. Invoke `callable_()` inside a try-catch.
2. No exception — assertion passes; return.
3. Catch `std::exception& e` — assertion fails: expected `"no exception"`,
   actual `e.what()`.
4. Catch `...` — assertion fails: expected `"no exception"`, actual
   `"unknown exception"`.

### Design Notes

- `F` is constrained with `std::invocable` at the class level.
- `callable_` is stored by value (moved in), same rationale as `Expect<T>`.
- Both methods return `void` — no chaining on callable expectations.
- `ToThrow` is not `constexpr` because it relies on RTTI (`typeid`) and
  `Demangle`.

## 6. Design Rationale Summary

| Decision | Choice | Rationale |
|---|---|---|
| Deducing `this` | `this auto&&` on all `Expect` methods | Handles lvalue and rvalue chaining; exercises C++23 |
| Value storage | `T actual_` / `F callable_` by value | Avoids dangling refs with temporaries |
| Per-method concepts | `equality_comparable`, `totally_ordered`, `convertible_to<bool>` | Clear compiler errors; unconstrained class stays flexible |
| Stringification | `formattable` primary, `operator<<` fallback, `<non-printable>` final | Widest coverage via C++23 `std::format` |
| constexpr | Constructor + methods marked `constexpr` | Compile-time assertion when possible; throw fails the constant evaluation |
| CTAD | Implicit deduction guides | `Expect(value)` and `ExpectCallable(callable)` just work |
| AssertionError fields | Public data members | Runner/output layer accesses directly; no getter boilerplate |
| Error format | `{file}:{line}: assertion failed\n  expected: ...\n    actual: ...` | Compact, IDE-clickable |

## 7. Usage Examples

```cpp
#include <flul/test/expect.hpp>
#include <flul/test/expect_callable.hpp>

using flul::test::Expect;
using flul::test::ExpectCallable;

// Value assertions
Expect(42).ToEqual(42);
Expect(1 + 1).ToNotEqual(3);
Expect(true).ToBeTrue();
Expect(0).ToBeFalse();

// Chaining
Expect(50).ToBeGreaterThan(0).ToBeLessThan(100);

// Callable assertions
ExpectCallable([] { throw std::runtime_error("boom"); }).ToThrow<std::runtime_error>();
ExpectCallable([] { /* no-op */ }).ToNotThrow();
```
