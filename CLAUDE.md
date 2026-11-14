# Heart Boom Maths

A mental-arithmetic game for ages 5 to 12. The player answers 10 number sentences per level against a heart-shaped bomb timer. v1 targets iPhone, iPad, Android, WebAssembly, macOS, Windows and Linux from one C++23 core inside a Qt 6 and QML shell.

Repository: `iainchesworthlabs/heart-boom-maths`. Licence: GPL-3.0-or-later.

## Read first

1. `docs/SPEC.md` for the game rules. If it conflicts with the PRD, the PRD wins.
2. `docs/game-architecture.md` for how the rules are built.
3. `docs/DECISIONS.md` for accepted decisions, open gaps (G1 to G8) and risks.
4. `docs/MILESTONES.md` for the order of work and the M0 and M1 briefs.
5. `docs/COMPLIANCE.md` before touching anything that stores, sends or opens something.
6. `private/DESIGN.md` and `private/design/boards/` before touching any screen. They are local only and never committed.
7. `AGENTS.md` for branches, commits, pull requests and the update-X-then-Y table, and `docs/architecture.md` for the
   template's platform-code and naming rules. Where `AGENTS.md` and this file differ on process, `AGENTS.md` wins. On
   a game rule, `docs/SPEC.md` and the PRD win.

Numbers live in `libs/core/data/tuning.json` and `libs/core/tests/data/prd-tables.json`. Do not hard-code a game number anywhere else.

## Rules that must not be broken

- `libs/core` includes the C++ standard library and nothing else. No Qt, file access, clock, threads or global state. CI fails on a Qt include in `libs/core`.
- Core state is integers only. No floating point in anything that is saved, hashed or replayed.
- Use the core's own PRNG and bounded-integer function. Never use `std::uniform_int_distribution` or other standard distributions.
- The core never reads a clock. The app layer turns real time into whole ticks (20 per second) and calls `tick()`.
- The app never recomputes a rule. It draws `view()` and reacts to events.
- Only `apps/game` may hold a clock, a `QObject` or a QML type.
- No network calls, analytics, crash reporting, ads, purchases, accounts, notifications or location. v1 must stay in the lowest age rating in each store.
- The only way to open a URL is `LinkOpener::open`, reached through the parental gate. CI checks that there is exactly one call.
- Names typed by children never leave the device.
- A bad or newer save file never crashes the game or gets overwritten. Writes are atomic.
- No cue depends on colour alone, and every sound has a visual twin.
- Fonts load from application resources, never from the network.

## Conventions

- C++23. Allman braces. PascalCase types, snake_case functions and local variables, `m_PascalCase` class members, UPPER_SNAKE constants. QML-facing names are camelCase.
- Non-C++ files (CMake, YAML, JSON, scripts, QML): complete and explicit. No placeholders, no "rest unchanged".
- Comments explain why, not what. Keep documentation short and do not repeat what another file already says.
- Prefer the standard library. A new dependency needs a licence file (CI checks) and a reason in the pull request.
- Match the existing pattern from `iainchesworthlabs/snake` and `icl-common-infra` for presets, vcpkg, versions file, CI tiers and licence files.

## Working agreement

- Trunk-based. `develop` is the fast trunk, `main` carries releases only, and a version tag on `main` starts the release build. Keep branches short.
- Commit messages and pull request text carry no attribution lines of any kind.
- Pull requests from forks get no secrets. Signing keys exist only in the tag build.
- Pin every GitHub Action by commit SHA.
- A rule or tuning change regenerates the affected golden replays in the same commit. A replay that changes without a deliberate change is a bug.
- Order of work is spec, core, then screens. Do not start `apps/game` screens before the core passes its tests.
- Spec decisions belong to the project owner and the game's designer. Ask before changing a game rule, a number in the PRD tables, or the look. Record the answer in `docs/DECISIONS.md`.
- The remote repository exists. Work goes through branches and pull requests as `AGENTS.md` describes, with Auto-fix and auto-merge on. Ask before tagging, changing repository settings or rulesets, spending money (a licence, a store account) or filing anything with a store.

## Layout

The repository follows the organisation template, as `snake` does: `libs/core` is the rules library (standard library only, tests in `libs/core/tests`, release values in `libs/core/data/tuning.json`), and `apps/game` is the Qt and QML application (QML in `qml/`, view-models and services in `src/`, per-OS code in `src/platform/<os>/`, fonts in `fonts/`). The handover's five top-level folders map onto these. `private/` is ignored by Git and holds the design spec and boards; read it for screen work and never commit it.
## Commands

Presets: `linux-debug`, `linux-release`, `linux-asan-ubsan`, `linux-tsan`, `linux-coverage`, `macos-release`, `windows-msvc-release`, `windows-msvc-asan`, `ios-release` (Mac only) and `wasm-release` (needs `EMSDK`; see `docs/building.md`).

```bash
cmake --preset linux-debug && cmake --build --preset linux-debug && ctest --preset linux-debug   # configure, build, test
ctest --preset linux-debug -L unit                          # one label: unit, integration, system, ui, perf
pwsh -NoProfile -File tools/dev/test.ps1 [-Filter <regex>]  # the same on Windows (loads MSVC; windows-msvc-release)
python3 tools/ci/precheck.py                                # the Policy checks plus the two project rules, before a push
python3 tools/ci/check_core_includes.py --self-test         # also check_link_open.py; each script has a self-test
```

`apps/game` builds only when a Qt 6 kit is found (`ICL_QT_ROOT`); set `-DHEART_BOOM_MATHS_BUILD_GAME=ON` to require it or `OFF` to skip it. Its tests are labelled `ui` (`game.smoke`, `game.qmllint`) and `integration` (`game.version`).

## Definition of done

Tests pass in the tier that applies. New behaviour has a test. `tuning` and replays are consistent. The screen matches its board, checked by hand. No new warnings. The change is small enough to review in one sitting.
