# ADR 0001: Test framework and JSON

Status: accepted. Milestone: M0.

## Context

The core is standard library only, so its tests must run without Qt or a display, and its data (the tuning numbers and
the save file) must not need a parser the core does not own. The organisation standard in `icl-common-infra` already
fixes the test tooling for every project.

## Decisions

**Catch2 with CTest.** The organisation standard requires it, and CI, coverage and the phased test runner are built on
CTest labels (`unit`, `integration`, `system`, `ui`, `perf`). Catch2 comes from vcpkg (the organisation's fork) and is
pinned by the baseline in `vcpkg.json`. It is a test-only dependency and never ships. Property tests are Catch2 cases
that run a fixed number of seeded iterations, so a failure repeats.

**The tuning JSON is read by CMake, not by the core.** `libs/core/data/tuning.json` is turned into a header by
`string(JSON)` when CMake configures (`cmake/HbmTuningData.cmake`), and `Tuning::defaults()` returns that constant data.
The core needs no parser for its release values, the numbers live in one data file, and a test pins them to the PRD
tables (`libs/core/tests/data/prd-tables.json`). Playtest overrides are the app layer's job (see
`game-architecture.md`, Tuning).

**The save file is read by a small hand-written module in `libs/core`.** The save is one JSON document, owned by the core
so every platform reads and writes the same file. The module handles only objects, arrays, strings, integers and
booleans, uses the standard library alone, and never throws on bad input: a damaged file is reported, not parsed
half-way. A general JSON library would add a dependency to the one layer that must have none, and would bring floating
point into state that is hashed and replayed. Round-trip and random-input tests cover it.

## Consequences

- A change to a game number is a change to a data file, and the PRD-table test fails until both agree.
- The save reader is project code and needs its own tests and fuzz-style input tests; those are part of M1.
- Nothing in `libs/core` links Qt, Catch2 (except its tests) or a JSON library; `tools/ci/check_core_includes.py` checks it.
