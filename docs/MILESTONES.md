# Milestones

The order is spec, then core, then screens. No screen work starts until the core passes its tests, because every screen only shows what the core decides. There are no dates, because the pace depends on the time available. A milestone ends when its exit criteria pass.

| Milestone | Work | Exit criteria |
|---|---|---|
| M0 Spec freeze | Settle gaps G1 to G8 in `DECISIONS.md` and write them into the PRD. Create the repository, presets, Tier 1 CI, licence files and the test framework. Load `libs/core/tests/data/prd-tables.json` in a test | Tier 1 is green on an empty core. No open PRD question blocks the core |
| M1 Core | `GameSession`, timer, scoring, generator, PRNG, `Tuning`, save logic, and a text-mode harness in `libs/core/tests/` | All core tests, replays and property tests pass in Tier 1. A full 12-level game and endless mode play through the harness |
| M2 Spikes | Run beside M1. Web spike: storage, page visibility, audio and download size. iOS spike: Qt static build, an Objective-C++ stub for Game Center, and the App Store licence check | Each spike has a written result and a decision to go on or change course |
| M3 App shell | `GameController`, tick driver, Play screen, pause and resume, file save and placeholder art, on desktop | A full run from level 1 to game over on desktop, laid out as boards CP2 to CP5. The Tier 2 matrix is green. This is the first build a child can play |
| M4 All screens | The remaining screens, parental gate, Comic Pop art, bundled fonts, audio, both size classes, `BuildInfo` and notices | Every board is built and checked by hand. App-layer tests run with no QML warnings |
| M5 Platforms | iOS, Android and WebAssembly builds on devices and in browsers, platform stores, lifecycle and screen-time handling, the Game Center switch | The game runs on a phone, a tablet and in a browser. The device tests in `COMPLIANCE.md` are done and recorded |
| M6 Playtest | Playtests with the target age group. Settle the 7 s and 3 s adjustments and the endless ramp | Decisions are recorded in the PRD and in `Tuning`, with replays regenerated |
| M7 Release | Signed builds, store listings and forms, the privacy policy page, desktop releases, and a tag on `main` | Apple and Google both approve the build, and `v1.0.0` is tagged |

## M0 brief

Do these in order. Ask Iain before creating the remote repository, pushing, or changing the PRD.

1. Read `CLAUDE.md`, `docs/SPEC.md`, `docs/game-architecture.md` and `docs/DECISIONS.md`.
2. Look at how `iainchesworthlabs/snake` and `iainchesworthlabs/icl-common-infra` set up CMake presets, the vcpkg fork, the versions file, CI tiers and licence files. Reuse them. Do not invent a second pattern. Note anything in this project that has to differ.
3. Propose answers to G1 to G8 (the suggestions in `DECISIONS.md` are the default) and ask Iain to confirm. Iain edits the PRD. Record the confirmed answers in `docs/DECISIONS.md`.
4. Replace the template's sample code with the project skeleton (`libs/core`, `apps/game`), `CMakeLists.txt`, `CMakePresets.json` with the presets listed in `game-architecture.md`, the versions file that pins Qt and Emscripten, `LICENSE` (GPL-3.0-or-later), and `.clang-format` for Allman braces.
5. Choose the test framework, pin it in vcpkg, and write down why in a short ADR in `docs/`.
6. Add a core target that links no Qt, and a CI check that fails when a core file includes a Qt header.
7. Add `libs/core/tests/tuning_test.cpp`: load `libs/core/tests/data/prd-tables.json` and `libs/core/data/tuning.json` and check they agree row by row, including the five endless anchor rows against the three endless functions.
8. Add Tier 1 CI on every push. Pin every action by commit SHA.

Done when Tier 1 is green on a core that does nothing yet and the tuning test passes.

## M1 brief

Build `libs/core` and its tests. Nothing in `apps/game` yet except what the tests need.

1. `Tuning` and `Tuning::defaults()`, loaded from the values in `libs/core/data/tuning.json`. A parser for `tuning.json` belongs in `libs/core` only if it stays standard-library only. Otherwise generate a header from the JSON at configure time.
2. The PRNG and a bounded-integer function. Test the distribution roughly and the exact sequence for fixed seeds. Fix the sequence in a test so any change is deliberate.
3. The question generator, with property tests: one correct tile, four distinct values, expression evaluates to the answer, operands and intermediates in range, no repeats within a level, levels 1 and 2 deal all 10 and 15 possibilities once.
4. `GameSession` with the states, commands and events in `game-architecture.md`. Implement the timer in ticks, scoring and coins, multiplier run and step, hints, level end, win, endless, and game over.
5. The JSON reader and writer, the save schema, migrations (version 1 only so far) and `MemorySaveStore`. Test round trip, damaged file, newer file, and random input.
6. A text-mode harness in `libs/core/tests/` that plays a game from a seed and a script, prints each question and event, and exits with the final score. It is how a human checks the rules before any screen exists.
7. Golden replays for a perfect level, wrong answers and hints, a time-out in the alert zone, the 12-level win, and endless mode to level 20. Add the 64-bit FNV-1a state hash.
8. Run everything under ASan and UBSan locally before opening the pull request.

Done when all Tier 1 tests pass, the replays are stored, and the harness plays a complete 12-level game and endless mode.

## Later briefs

Write the M2 to M7 briefs when each milestone starts, from the table above and the plan. The M2 spikes can start as soon as M0 is done.
