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

```cpp
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
```

Public data members allow the Runner and output layer to access fields directly.
`what_` is built eagerly in the constructor with the format
`{file}:{line}: assertion failed` followed by expected/actual lines. The
`{file}:{line}:` prefix is IDE-clickable. The class is not a template — `actual`
and `expected` are always pre-stringified by the caller.

## 3. `Stringify`

Three free-function overloads, selected by concept constraints (most constrained
wins):

```cpp
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
```

The three overloads are mutually exclusive via negated constraints — no ambiguity.

### `Demangle`

```cpp
auto Demangle(const char* mangled) -> std::string;
```

Wraps `__cxa_demangle` (available on macOS/Linux via `<cxxabi.h>`). Used by
`ExpectCallable` to produce readable type names in error messages.

## 4. `Expect<T>`

```cpp
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
```

**Deducing `this`** — `this auto&&` handles both lvalue and rvalue chains,
returning `std::forward<decltype(self)>(self)` to preserve value category.

**Value storage** — `T actual_` stored by value (moved from constructor).
Avoids dangling references when constructed with a temporary.

**Per-method constraints** — Each method constrains only what it needs.
`Expect<T>` itself is unconstrained, so it can be instantiated for any `T`.

**constexpr** — Constructor and all methods are `constexpr`. If an assertion
fails at compile time, the `throw` makes the evaluation non-constant, producing
a compiler error at the call site.

**CTAD** — `Expect(value)` deduces `T` via implicit class template argument
deduction.

All methods throw `AssertionError{actual_str, expected_str, loc_}` on failure,
using `Stringify` for the string representations.

## 5. `ExpectCallable<F>`

```cpp
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
```

`F` is constrained with `std::invocable` at the class level. `callable_` is
stored by value (same rationale as `Expect<T>`). Both methods return `void` —
no chaining on callable expectations.

**`ToThrow<E>`** — invokes callable, catches `E&` (pass), catches `...`
(fail: "different exception"), or no exception (fail: "no exception"). Uses
`Demangle(typeid(E).name())` for readable type names. Not `constexpr` due to
RTTI dependency.

**`ToNotThrow`** — invokes callable. No exception means pass. Catches
`std::exception& e` (fail: `e.what()`) or `...` (fail: "unknown exception").

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
| Error format | `{file}:{line}: assertion failed` + expected/actual | Compact, IDE-clickable |

## 7. Usage Examples

```cpp
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
