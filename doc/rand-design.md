# Randomized Execution Order — Detailed Design

## 1. Overview

Randomized execution order (`#RAND`) shuffles the sequence in which tests are
executed, exposing hidden ordering dependencies. It is opt-in via `--randomize`
or `--seed <N>` CLI flags. The seed is printed before any test output so a
failing order can be reproduced exactly. Shuffling happens after all filtering
(name, tag include, tag exclude) and before execution, affecting only the
surviving test set. Listing (`--list`, `--list-verbose`) always uses
registration order and is unaffected.

## 2. Requirements Reference

From `doc/requirements.md`, feature `#RAND`:

- Default is fixed (deterministic) order; opt-in via `--randomize`
- Randomization at two levels: suite order, and test order within each suite
- Seed printed at start of run in format `[seed: N]`, before any test output
- `--seed <N>` accepts `uint32_t` range (0-4294967295)
- `--randomize` with `--seed <N>` uses the provided seed
- `--seed <N>` without `--randomize` implicitly enables randomization
- Out-of-range or non-integer `--seed` value: non-zero exit + descriptive error
- Filtering happens first, then randomization on surviving tests
- `--list` and `--list-verbose` unaffected by `--randomize` / `--seed`
- Output order matches execution order

## 3. Design Goals

- Zero impact on existing non-randomized runs (no overhead, no output change)
- Reproducible: same seed + same test set = same order, guaranteed
- Fail fast on invalid `--seed` input with clear error message
- Two-level shuffle (inter-suite, intra-suite) to thoroughly probe ordering
- Minimal API surface: one new method on `Registry`, two new CLI flags

## 4. C++ Architecture

### 4.1 `Registry::Shuffle` (new method: `include/flul/test/registry.hpp`)

```cpp
void Shuffle(uint32_t seed);
```

Shuffles `entries_` in place using a two-level algorithm:

1. Group entries by `suite_name` (preserving relative order within each group).
2. Shuffle the order of groups using `std::mt19937` seeded with `seed`.
3. Shuffle entries within each group using the same RNG (continuing its state).
4. Flatten back into `entries_`.

Using a single `std::mt19937` instance (seeded once) ensures the entire
permutation is deterministic from the seed alone. The grouping step uses the
existing `suite_name` field on `TestEntry` (or `TestMetadata::suite_name` once
`#TAG`/`#XFAIL` land).

`std::mt19937` is chosen because it is deterministic across platforms (required
by the C++ standard), unlike `std::random_device` or platform-specific RNGs.

### 4.2 Seed generation and validation (in `Run()`)

Seed parsing lives in `Run()`, not in `Registry`. Two scenarios:

- **`--randomize` without `--seed`**: generate a seed via
  `std::random_device{}()` masked to `uint32_t`.
- **`--seed <N>`** (with or without `--randomize`): parse `N` as `uint32_t`.

**Validation of `--seed <N>`:**

1. Missing argument after `--seed`: error + exit 1.
2. Parse with `std::from_chars` (base 10, full string must be consumed).
3. If `from_chars` fails (non-integer, partial parse): error + exit 1.
4. If the parsed value exceeds `uint32_t` range: parse into `uint64_t` first,
   then range-check. Error + exit 1 if out of range.
5. Leading `+` or `-` signs: `from_chars` with `uint64_t` rejects `-`;
   `+` is also rejected since the full string is not consumed.

Error message format:
```
error: --seed requires a non-negative integer in range 0-4294967295, got '<input>'
```

### 4.3 Seed output (in `Run()`)

When randomization is active, `Run()` prints `[seed: N]` to stdout before
creating the `Runner`. This is before any `[ PASS ]` / `[ FAIL ]` output.
The seed line uses `std::println("[seed: {}]", seed)`.

When randomization is not active, nothing is printed.

### 4.4 `Run()` execution order (modified: `include/flul/test/run.hpp`)

Updated pipeline in `Run()`:

1. Parse all CLI args (collect `--filter`, `--tag`, `--exclude-tag`,
   `--randomize`, `--seed`, `--list`, `--list-verbose`, `--help`)
2. Validate `--seed` value (fail fast if invalid)
3. `registry.Filter(pattern)` if `--filter`
4. `registry.FilterByTag(include_tags)` if any `--tag` flags
5. `registry.ExcludeByTag(exclude_tags)` if any `--exclude-tag` flags
6. If `--list`: `registry.List()`, return 0
7. If `--list-verbose`: `registry.ListVerbose()`, return 0
8. If randomization active:
   a. `std::println("[seed: {}]", seed)`
   b. `registry.Shuffle(seed)`
9. `Runner(registry).RunAll()`

Steps 6-7 are before step 8: listing always reflects registration order (post-
filter but pre-shuffle). This satisfies the requirement that `--list` and
`--list-verbose` are unaffected by randomization flags.

### 4.5 `Runner` (unchanged)

`Runner::RunAll()` iterates `registry_.Tests()` in whatever order the vector
currently holds. After `Shuffle`, that order is randomized. No changes needed
to `Runner` — output order naturally matches execution order because results
are printed immediately after each test runs.

### File Map

| File | Change |
|------|--------|
| `include/flul/test/registry.hpp` | **Modified** — add `Shuffle(uint32_t)` |
| `include/flul/test/run.hpp` | **Modified** — `--randomize`, `--seed` flags, seed output, `Shuffle` call |

### Namespace

All types remain in `flul::test`. No new namespaces.

## 5. Key Design Decisions

### `std::mt19937` for deterministic cross-platform shuffling

`std::shuffle` with `std::mt19937` guarantees identical permutations across
compilers and platforms for a given seed. `std::default_random_engine` does not
have this guarantee. `mt19937` is the lightest standard engine that provides
determinism.

**Trade-off:** `mt19937` state is 2.5 KB. Acceptable — it is created once per
run and destroyed immediately after shuffling.

### Two-level shuffle (suites then tests) vs flat shuffle

A flat shuffle of all entries would intermix tests from different suites. The
two-level approach shuffles suite order and test order within each suite
independently. This is more useful for detecting ordering dependencies: it
catches both inter-suite and intra-suite coupling while keeping related tests
loosely grouped in output.

**Trade-off:** Slightly more complex than a single `std::shuffle`. Worth it
because inter-suite ordering bugs and intra-suite ordering bugs are different
failure modes.

### Seed validation via `std::from_chars` (not `stoul` or `strtoul`)

`std::from_chars` is locale-independent, does not throw, and reports partial
consumption. `stoul` silently accepts leading whitespace and `+` signs, and
throws on error. `from_chars` gives precise control over what is accepted.

### `--seed` implicitly enables randomization

Requiring both `--seed 42` and `--randomize` to replay an order is a usability
trap. `--seed` alone is sufficient. `--randomize --seed 42` also works (same
behavior). This follows the principle of least surprise.

### Seed printed to stdout (not stderr)

The `[seed: N]` line is part of the test run output, not a diagnostic. Users
capture stdout to reproduce failures. Printing to stderr would lose the seed
when stdout is redirected to a file.

### `Shuffle` mutates `entries_` in place

Consistent with `Filter`, `FilterByTag`, and `ExcludeByTag` which also mutate
in place. No need for a separate shuffled view — the vector is consumed once
and the `Registry` is not reused after `Run()`.

## 6. Feature Changelog

Initial version. No prior `doc/rand-design.md` exists.

**Breaking changes to existing code:** None. New CLI flags and a new method on
`Registry`. Existing behavior is unchanged when flags are not used.
