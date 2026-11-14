# Testing

Tests use Catch2 through CTest. Each test carries one label that names its kind: `unit`,
`integration`, `system`, `ui` or `perf`. CI runs them in that order, with `unit`,
`integration` and `system` in parallel, `ui` at two jobs, and `perf` last on its own.
A failed test is run once more by name; a pass on the second run is reported as a warning so
flaky tests stay visible. A phase that matches no tests fails, unless the phase is marked optional.

| Kind | Where | Example |
|---|---|---|
| unit | `libs/*/tests` | `test_tuning.cpp` |
| integration | `apps/game` (in `CMakeLists.txt`) | `game.version` runs the program and reads its answer |
| unit, integration | `apps/game/tests` | `heart_boom_maths_game_tests`, Catch2 on the offscreen platform: the tick driver, the parental gate, the controller with a manual clock, the file save store. A case tagged `[unit]` or `[integration]` gets that label, and a Qt warning outside the game's own `hbm.game` log category aborts the test |
| unit | `apps/game` (in `CMakeLists.txt`) | `game.audio_placeholders` runs `tools/assets/make_placeholder_sounds.py --check`; it is added only when Python 3 is found |
| ui | `apps/game` (in `CMakeLists.txt`) | `game.smoke` starts the program on the offscreen platform and waits for the first frame; `game.qmllint` |

The `game.*` tests exist only when a Qt kit was found. On Windows the Qt `bin` directory must be on `PATH`;
`tools/dev/test.ps1` adds the `bin` directory of `ICL_QT_ROOT`. The scripts in `tools/ci/` carry their own tests: run
`python3 tools/ci/check_core_includes.py --self-test` and the same for `check_link_open.py`.

## Golden replays and the text harness

A replay is a JSON file of a seed and the commands of a whole run, each with the tick it arrives on, plus the events hash,
the final state hash and the score it must produce (format in `docs/game-architecture.md`; `end_tick` lets a run end by
letting the clock run out, and `recipe` records how the file was made). The files live in
`libs/core/tests/data/replays/`. CMake turns each into a byte array in a generated header, so the `[replay]` test reads no
file at run time.

`heart_boom_maths_harness` (built with the tests, from `libs/core/tests/harness.cpp`) plays a game in the terminal from a
seed and either a script or a policy, printing each question, the tiles, every event in words, the clock and the score.
The last line is always `FINAL score=<n> level=<n> events_hash=<hex> final_hash=<hex>`.

```bash
harness --auto perfect --seed 7                         # a whole 12-level game, perfect play
harness --auto perfect --endless-to 20                  # win, KEEP GOING, on to level 20
harness --auto random 42 --wallet 6 --clear-level 2     # right, wrong, hints and pauses from a second generator
harness --script some-replay.json                       # replay a file; says whether its hashes still match
harness --auto always-wrong --record wrong.json         # also write the run as a replay file
```

A rule or tuning change regenerates the affected replays in the same commit:

```bash
harness --regenerate libs/core/tests/data/replays       # rewrites each file from its recipe
harness --check libs/core/tests/data/replays            # what the ctest test harness.replays-current runs
```

Review the diff of the replay files like any other change. A replay that changes when no rule or tuning was meant to change
is a bug in the change, not a file to regenerate. The `[replay]` test failure says so and names this command.

## Coverage

`linux-coverage` builds with instrumentation. CI runs gcovr and `coverage_gate.py` against
`tools/coverage-floors.json`. The target for every component is 90% statement and 80% branch, set once
as `default` with no per-component overrides. A component that is temporarily below the target is
listed with its measured value and raised in the same pull request as the tests that lift it, until it
reaches the default. The gate prints a hint when a component is above its floor; raise the floor in the same
pull request so coverage only moves up.

A line that cannot be reached by a test, such as a failing system call, is excluded with a
`GCOVR_EXCL_BR_LINE` comment and a reason.

## Running one kind

```bash
ctest --preset linux-debug -L unit
```
