# Architecture

```text
libs/core/            the game rules: standard library only, no Qt; Tracy only through include/heart_boom_maths/profile.hpp (the layers are in game-architecture.md)
  include/heart_boom_maths/
  src/
  data/tuning.json    release values; CMake turns them into a header when it configures
  tests/              Catch2 tests, and data/prd-tables.json (the PRD's own numbers)
apps/game/            the Qt 6 and QML application
  qml/                screens
  src/                view-models and the tick driver
    services/         service interfaces and shared code
    platform/<os>/    the per-OS implementations, chosen by CMake
  fonts/              Bangers and Lilita One with their licence texts
apps/ios/             the iPhone and iPad packaging: a one-screen UIKit stub until Qt runs on iOS (see M2)
  packaging/          Info.plist.in, asset catalogue (icon, launch colour), verify_app.sh
cmake/icl/            vendored CMake modules from icl-common-infra (do not edit)
cmake/                the project's own CMake (HbmTuningData.cmake)
tools/                project scripts, baselines and coverage floors
docs/adr/             decision records for choices that are not game rules
```

`apps/game` is built when a Qt 6 kit is found, so a machine with no Qt still builds and tests the core.
`HEART_BOOM_MATHS_BUILD_GAME` is `AUTO` (build it if Qt is found), `ON` (a missing kit is an error) or `OFF`.

## Platform code

Platform differences live in `apps/game/src/platform/<os>/` with the same file names in each directory.
`icl_platform_sources(target)` adds the directory that matches the target system. The core has none: it does not
touch the operating system. Source files contain no `#if` on the platform or compiler; the preprocessor check
fails the pull request if one appears. Other directives are counted per file against
`tools/preprocessor-baseline.json`, which can go down but not up.

## Rules CI checks

| Rule | Check |
|---|---|
| `libs/core` includes the C++ standard library and nothing else, and no Qt; Tracy only through `heart_boom_maths/profile.hpp`, which is empty unless profiling is built in | `tools/ci/check_core_includes.py` |
| One call opens a URL, behind the parental gate | `tools/ci/check_link_open.py` |

Both run in the `Project rules` job and in `python3 tools/ci/precheck.py`. Each has a `--self-test`.

## Naming

Namespaces, functions and variables use `snake_case`. Types use `PascalCase`. Members use
`m_PascalCase`. Constants and enumerators use `UPPER_SNAKE_CASE`. Braces follow Allman style.
`.clang-format` and `.clang-tidy` enforce this.

## Vendored modules

`cmake/icl/` is a copy of `icl-common-infra/cmake/`. `cmake/icl/SOURCE` records the commit.
CI compares the copy with the commit its workflows pin and fails on a difference. Update with
`python3 tools/ci/sync_cmake.py --infra <checkout of icl-common-infra>`.

The game's own design (core API, persistence, services, tests, CI tiers) is in [game-architecture.md](game-architecture.md).
