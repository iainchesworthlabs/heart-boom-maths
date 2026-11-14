# Testing

Tests use Catch2 through CTest. Each test carries one label that names its kind: `unit`,
`integration`, `system`, `ui` or `perf`. CI runs them in that order, with `unit`,
`integration` and `system` in parallel, `ui` at two jobs, and `perf` last on its own.
A failed test is run once more by name; a pass on the second run is reported as a warning so
flaky tests stay visible. A phase that matches no tests fails, unless the phase is marked optional. The `perf` tests are
not built by the ordinary presets, so that phase finds none here; they run in their own build and job (see Performance
tests below).

| Kind | Where | Example |
|---|---|---|
| unit | `libs/*/tests` | `test_tuning.cpp` |
| integration | `apps/game` (in `CMakeLists.txt`) | `game.version` runs the program and reads its answer |
| unit, integration | `apps/game/tests` | `heart_boom_maths_game_tests`, Catch2 on the offscreen platform: the tick driver, the parental gate, the controller with a manual clock, the file save store, the Game Center score service over a fake GameKit backend. A case tagged `[unit]` or `[integration]` gets that label, and a Qt warning outside the game's own `hbm.game` log category aborts the test |
| unit | `apps/game` (in `CMakeLists.txt`) | `game.icon_placeholder` runs `tools/assets/make_placeholder_icon.py --check` on the iOS placeholder icon, and `game.audio_placeholders` runs `tools/assets/make_placeholder_sounds.py --check`; both are added only when Python 3 is found |
| ui | `apps/game` (in `CMakeLists.txt`) | `game.smoke` starts the program on the offscreen platform and waits for the first frame; `game.qmllint` |
| ui, unit | `apps/game/tests` | `heart_boom_maths_ui_tests`, Catch2 with a `QGuiApplication` on the offscreen platform and the software renderer. It loads the shell and the play screen at phone, tablet and desktop sizes, drives the controller with a manual clock and reads the QML object tree. A Qt or QML warning aborts the test. A case tagged `[ui]` or `[unit]` gets that label. `heart_boom_maths_ui_tests "[shots]"` with `HBM_SHOT_DIR` set writes screenshots for the hand check against the boards; they are not committed |
| perf | `libs/core/tests/perf`, `apps/game/tests/perf` | `heart_boom_maths_core_perf`, `heart_boom_maths_game_perf` and the optional `heart_boom_maths_game_perf_gpu`: speed and memory against budgets. Built only by the `*-perf` presets |
| unit, integration | `libs/core/tests`, `apps/game` (in `CMakeLists.txt`) | Profiling ([Profiling](profiling.md)): the `[profile]` cases check that the profiling marks are real exactly when the build has Tracy and that a replay gives the same events and hashes with the profiler off, started and stopped; `game.tracy_binary` and `game.tracy_in_core` run `tools/ci/check_binary_tracy.py` over the program and the core (no Tracy in a build without it, Tracy in one with it); `game.no_listener_when_not_asked` and `game.loopback_listener_when_asked` run `tools/ci/check_listener.py` (with the switch unset nothing listens, with it set one listener opens, on the loopback interface only, once); `game.profiler_*` start the program with the flag and the variable. `tracy.refused_for_*` run the configure check as a WebAssembly, Android and iOS build would meet it (Tracy is refused); `tools/ci/check_presets_tracy.py`, also a project rule in CI, keeps Tracy out of the web, Android, iOS, sanitizer and coverage presets |

The `game.*` tests exist only when a Qt kit was found. On Windows the Qt `bin` directory must be on `PATH`;
`tools/dev/test.ps1` adds the `bin` directory of `ICL_QT_ROOT`. The scripts in `tools/ci/` carry their own tests: run
`python3 tools/ci/check_core_includes.py --self-test` and the same for `check_link_open.py` and `check_presets_tracy.py`.

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
`tools/coverage-floors.json`. The target for every component is 90% statement and 80% branch. The gate needs
each component listed by its path, so `libs/core` and `apps/game` appear with the same 90 and 80 as `default`. A
component that is temporarily below the target is listed with its measured value and raised in the same pull request as
the tests that lift it, until it reaches the target. The gate prints a hint when a component is above its floor; raise the floor in the same
pull request so coverage only moves up.

A line that cannot be reached by a test, such as a failing system call, is excluded with a
`GCOVR_EXCL_BR_LINE` comment and a reason.

## Nightly checks

`.github/workflows/nightly.yml` runs what is too slow or too noisy for every pull request, once a night against `develop`
(02:37 UTC, or `gh workflow run nightly.yml --ref <branch>`; `-f jobs=cppcheck,clang-tidy` runs only those groups, which are
`sanitizers`, `clang-tidy`, `cppcheck`, `gcc-analyzer`, `msvc-analyze` and `dependencies`). It is not a required check and
cannot block a merge. The per-pull-request sanitizer and coverage jobs stay as they are. `docs/ci.md`, "Nightly", lists the
jobs with their runners; CodeQL is `codeql.yml`.

### Sanitizers

Each leg builds the preset, then runs every test label but `perf` and `fuzz` (`.github/ci/nightly-phases.json`: `unit`,
`integration` and `system` in parallel, `ui` at two jobs on the offscreen platform, anything else last, and a failed test
run once more by name so that a flaky pass is a warning). The core's replay and property tests are the heavy ones.

| Leg | Preset | Catches |
|---|---|---|
| Clang ASan -O1 | `linux-asan-o1` | Out-of-bounds access, use after free, return or scope, double free, and leaks (LeakSanitizer is part of ASan on Linux) |
| Clang UBSan | `linux-ubsan` | Signed overflow, bad shifts, null and misaligned access, invalid enum or bool values, a float cast out of range; the first report fails the run |
| Clang TSan | `linux-tsan` | Data races and lock-order inversions |
| GCC ASan and UBSan | `linux-asan-ubsan` with GCC | The same as Clang's, from GCC's runtimes |
| MSVC ASan /O1 | `windows-msvc-asan` | The ASan checks above on Windows (MSVC has no UBSan, no TSan and no leak check) |
| Apple Clang ASan and UBSan | `macos-asan-ubsan` | The same on macOS, with Apple's runtimes |

Every sanitizer build is optimised at `-O1` (`/O1` on MSVC) with frame pointers kept: `cmake/icl/IclSanitizers.cmake` adds
the flags to every sanitizer preset, which keeps the stacks readable and the run short. `linux-asan-o1` and `linux-ubsan`
also drop `-DNDEBUG`, so every `assert` runs. The existing `linux-asan-ubsan` preset is therefore -O1 as well, not -O0.
MSVC's AddressSanitizer works at any optimisation level, `/Od` included; the optimised build is the faster and is the one used.

The run-time options are the `env` block at the top of `nightly.yml`. Set the same three for a run by hand:

```bash
export ASAN_OPTIONS=detect_leaks=1:strict_string_checks=1:detect_stack_use_after_return=1:check_initialization_order=1
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
export TSAN_OPTIONS=halt_on_error=1:second_deadlock_stack=1:ignore_noninstrumented_modules=1
```

On Windows and macOS leave `detect_leaks=1` out: those runtimes have no leak check. `ignore_noninstrumented_modules=1` makes
TSan ignore what Qt's prebuilt libraries do. They are not built with TSan, it cannot see their locks, and every hand-over of
an object to a Qt thread would be reported as a race on the allocator. A race between this project's own threads is still
found, because the project's code is instrumented. There is no suppressions file; if a report ever has to be hidden, add
`tools/sanitizers/tsan.supp` with the reason on every entry and name it in `TSAN_OPTIONS`.

Run a leg by hand on Linux or in WSL (Clang 23 and a Qt kit; set `CC` and `CXX`, and `ICL_QT_ROOT` or `CMAKE_PREFIX_PATH`):

```bash
cmake --preset linux-asan-o1 && cmake --build --preset linux-asan-o1
ctest --test-dir build/linux-asan-o1 -LE 'perf|fuzz' -j8 --output-on-failure
```

and the same with `linux-ubsan`, `linux-tsan` and `linux-asan-ubsan`. Without a Qt kit add `-DHEART_BOOM_MATHS_BUILD_GAME=OFF`
to test the core alone. On Windows:

```powershell
pwsh -NoProfile -File tools/dev/test.ps1 -Preset windows-msvc-asan
```

which also runs the `perf` label. To leave it out, build the preset and run
`ctest --test-dir build/windows-msvc-asan -LE "perf|fuzz"` from a Visual Studio developer prompt with the Qt `bin` directory on
`PATH`. A kernel with wide address randomisation can stop a sanitizer at start ("unexpected memory mapping"):
`sudo sysctl -w vm.mmap_rnd_bits=28`.

### Static analysis and the baseline

| Job | Tool and configuration | Catches |
|---|---|---|
| clang-tidy | `.clang-tidy`: `bugprone`, `clang-analyzer`, `performance`, `portability`, and chosen `cert`, `concurrency`, `cppcoreguidelines`, `misc`, `modernize` and `readability` checks, plus the naming rules. The file says which noisy checks are left out and why | Bugs, ownership slips, wasted copies, names that break the conventions |
| cppcheck | `--enable=warning,performance,portability --std=c++23 --inline-suppr`; suppressions in `tools/static-analysis/cppcheck-suppressions.txt` | A second opinion from a different engine: bad conditions, uninitialised members, arguments passed the wrong way |
| GCC analyzer | `-fanalyzer` over `libs/core` (preset `linux-gcc-analyzer`) | Null dereference, use after free, leaks and uninitialised values on the paths the compiler follows |
| MSVC analyze | `/analyze` with the C++ Core Guidelines checker (preset `windows-msvc-analyze`) | Buffer and arithmetic overflow, uninitialised use, Core Guidelines violations. Five rules that ask for the Guidelines Support Library (`gsl::at`, `gsl::span` and the like) are off, as the preset's description says |

A tool's output is read by `tools/ci/static_analysis.py`. It counts findings per check and file (a count does not move
when a line does) and compares them with `tools/static-analysis-baseline.json`, the findings that are accepted for now.
The job fails only when a count is higher than the baseline allows, or a check and file are not in it, and says which. It
writes SARIF, which the job uploads to code scanning (one category per tool) and keeps as an artifact with the log. A finding
in the baseline is a to-do item, not an approval: code scanning shows it too.

```bash
python3 tools/ci/static_analysis.py check  --tool clang-tidy --log clang-tidy.log   # what the job does
python3 tools/ci/static_analysis.py lower  --tool clang-tidy --log clang-tidy.log   # after fixing some: lower the baseline
python3 tools/ci/static_analysis.py accept --tool clang-tidy --log clang-tidy.log   # take today's counts, whatever they are
```

`--tool` is `clang-tidy`, `cppcheck`, `gcc-analyzer` or `msvc-analyze`. To lower the baseline, fix findings, run `lower` on
the log of a run that has the fix and commit the smaller file in the same change; the job's summary prints the command when
the baseline is higher than the findings. `lower` never raises a count. `accept` does, which is how a finding that cannot
be fixed is taken in: the diff of the file is what a reviewer reads, and the reason belongs in the pull request. Prefer a
narrow `// NOLINT(check)` or `// cppcheck-suppress id ; reason` at the line, with the reason, to an entry in the baseline.

To run a tool by hand, configure and build its preset (the build makes the generated sources), then:

```bash
# clang-tidy: every file of ours in the compile database, four at a time
cmake --preset linux-static-analysis && cmake --build --preset linux-static-analysis
python3 tools/ci/static_analysis.py files --compile-commands build/linux-static-analysis/compile_commands.json \
  | xargs -P "$(nproc)" -n 4 clang-tidy -p build/linux-static-analysis --quiet > clang-tidy.log 2>&1
# cppcheck: the command of the "Run cppcheck" step in nightly.yml
# GCC analyzer: CXX=g++. Use GCC 16, the CI's: GCC 14 reports false uses of uninitialised values in the standard library
cmake --preset linux-gcc-analyzer && cmake --build --preset linux-gcc-analyzer -- -k 0 2>&1 | tee gcc-analyzer.log
```

```powershell
# MSVC analyze, from a Visual Studio developer prompt
cmake --preset windows-msvc-analyze
cmake --build --preset windows-msvc-analyze -- -k 0 2>&1 | Tee-Object msvc-analyze.log
python tools/ci/static_analysis.py check --tool msvc-analyze --log msvc-analyze.log
```

The baseline was counted with Clang 19 (clang-tidy), cppcheck 2.17, GCC 16.2 and MSVC 14.51; the CI uses Clang 23 and the
distribution's cppcheck, so the first night can differ. When the CI and a machine disagree, the CI is right: read its log artifact.

### Dependencies

The `dependencies` job reads every vcpkg port that `vcpkg.json` asks for, down through the ports' own dependencies, in the
checkout of the vcpkg fork at the baseline. It fails when a port has no licence or one that cannot be combined with
GPL-3.0-or-later (`tools/ci/dependency_audit.py licences`; the accepted SPDX identifiers are listed in that file). It runs
`pip-audit` (pinned in `requirements/audit.txt`) on `requirements/docs.txt` and fails on a known vulnerability. It prints how
far the vcpkg baseline, the ports, Qt, CMake and the Android NDK are behind the newest release, and that report never fails.
No vulnerability database covers vcpkg ports (OSV has no vcpkg ecosystem), so Dependabot's vcpkg updates and security alerts
are what flag one. By hand: `python3 tools/ci/dependency_audit.py licences --vcpkg-root <checkout at the baseline>` and
`pip-audit -r requirements/docs.txt`.

### Reading a failure

Open the run. **Nightly status** has a table of every job and leg with its time and a link, and opens (or comments on) one
issue labelled `nightly`, which it closes when a night is green. A red sanitizer leg has the report in its log, with the
stack; a failed test keeps `Testing/Temporary` as the `sanitizer-logs-<preset>` artifact for two weeks. A red analyser job has
the new findings in its summary and the whole log and SARIF as an artifact. A report that appears on one sanitizer leg only
is usually that runtime's: look for a frame in this project's code before suspecting Qt.

## Running one kind

```bash
ctest --preset linux-debug -L unit
```

## Performance tests

The tests labelled `perf` catch a regression in speed or memory. They are **not part of the ordinary run**: the option
`HEART_BOOM_MATHS_BUILD_PERF_TESTS` is off in every preset but the two `*-perf` ones, so `ctest` and the pull request gate
neither build nor run them, and a busy machine cannot fail a change that slowed nothing down. They have a build directory of
their own, run one test at a time, and run nightly in `perf.yml` and by hand.

```bash
pwsh tools/dev/test.ps1 -Perf          # Windows: builds windows-msvc-release-perf, runs label perf, prints the summary
cmake --preset linux-release-perf && cmake --build --preset linux-release-perf
ctest --preset linux-release-perf      # one PERF line per measurement
python3 tools/ci/perf_report.py        # the table, summary.json and summary.csv in <build>/perf-results
```

**What they measure.** Each measurement is the median and the best of nine repetitions after a warm-up, over enough work to be
stable (`std::chrono::steady_clock` in the test, never in the core).

| Program | Measures |
|---|---|
| `heart_boom_maths_core_perf` (`libs/core/tests/perf`) | Ticks per second of a running session (a million ticks of perfect play through the campaign and into endless play, with answers). Questions dealt per second at levels 1, 2, 5, 12, 13, 20 and 100. `snapshot()`, `restore()` and `state_hash()` per call at a campaign and an endless level. Writing and reading a full save (a top 10 and a paused run) and the JSON under it. The hostile 256 KiB inputs: widest array, most members, deepest nesting, too large, too long a string. Parsing and playing every golden replay, which includes the 12-level game and the endless run to level 20. The heap allocations per tick, per answer and per question, counted by a replacement of the global `operator new` that only this program has |
| `heart_boom_maths_game_perf` (`apps/game/tests/perf`) | `GameController::advance_time` per frame, and `answer()` with its signals, on a manual clock. The controller's start-up. The save file's write and read, and `persist()`. The time from asking for each screen (Title, Play at phone, tablet and desktop size, Level clear, Name entry, Top 10, About, and the notices text) to its first polished frame. The frame time of the Play screen at three sizes while a level plays with the held feedback animations, paced at 60 Hz (mean, p95, p99, max and the number of frames over 16.7 ms). A hundred cycles through the screens, which must leave the number of QML objects where it was and the process within a few megabytes |
| `heart_boom_maths_game_perf_gpu` | The optional GPU test: the Play screen on the machine's own renderer, with the GPU time of each frame from `QRhiCommandBuffer::lastCompletedGpuTime()`. It reports and never fails, and it skips itself, with the reason, unless `HBM_PERF_GPU=1`, there is a window system (not `offscreen`) and the graphics API gives timestamps |

The game's numbers are for the **software renderer on the offscreen platform** (their names end in `.software`), as for the
`ui` tests: they measure the QML and the C++, not a phone's GPU. A screen's time is its first visit in a fresh QML engine;
the frame times are of the warm screen. The game's programs do not count allocations: Qt's libraries free what the program
allocates, so replacing `operator new` there would crash them.

**The result line and the files.** Each measurement prints one line,
`PERF name=... metric=... unit=... median=... best=... budget=... pass=...` (any extra numbers follow as `key=value`), and
writes `<build>/perf-results/<name>.json`. `tools/ci/perf_report.py` reads them all, prints the table, writes `summary.json`
and `summary.csv` for tracking from one night to the next, and exits 1 when anything is over its budget.

**Budgets.** `tools/perf-budgets.json` holds one entry per measurement: name, metric, unit, the budget, the median when it
was set and the machine it was set on. A test fails only when its **median** is over its budget, or has no budget. They are
set with room on purpose: four times the median for a time, twice for a count, and never below 100 ns, 50 us or 4 MiB, so a
slower runner passes and only a real regression (several times slower, one more allocation per tick, a screen that leaks)
fails. A count of zero stays zero: `core.alloc.per_tick` and `app.leak.objects_after_100_cycles` are exact.

To retune after a deliberate change, or for a new class of machine, run the perf tests locally with the budget check off and
let the script rewrite the file:

```bash
HBM_PERF_NO_BUDGET=1 pwsh tools/dev/test.ps1 -Perf          # reports, never fails
python tools/ci/perf_report.py --update-budgets --results build/windows-msvc-release-perf/perf-results \
    --machine "describe this machine"
```

`--update-budgets` refuses to run when `CI` or `GITHUB_ACTIONS` is set. A new test gets its entry the same way. The diff of
`perf-budgets.json` (one entry to a line) is reviewed like any other change; say in the pull request why a budget rose.

**Baseline.** Set on an AMD Ryzen 9 5950X (16 cores, 32 threads), Windows 11, MSVC Release, Qt 6.12.0, software renderer. The
machine was shared with other builds while it ran, so the numbers are generous rather than best-case. A Linux runner's numbers
are not known yet: the first nightly run is its baseline, and the budgets are retuned from it if it needs that.

| Measurement | Median | Budget |
|---|---|---|
| Session tick (an answer every 2 s of play) | 16 ns (62 million ticks a second) | 100 ns |
| Allocations per tick, per answer | 0, 2 | 0, 4 |
| Deal a question at level 1, 5, 12, 20, 100 | 103, 184, 198, 211, 219 ns | 420 to 880 ns |
| `snapshot()`, `restore()`, `state_hash()` | 137 ns, 1.0 to 1.2 us, 385 ns | 0.55 us, 4.2 to 4.8 us, 1.6 us |
| `serialise`, `parse_save` of a full save | 4.6 us, 7.7 us | 19 us, 31 us |
| Hostile 256 KiB: widest array, most members | 6.6 ms, 7.9 ms | 27 ms, 32 ms |
| Hostile 256 KiB: deepest nesting, too large | 0.46 us, 8 ns | 1.9 us, 100 ns |
| Replay run: the 12-level game, endless to level 20 | 143 us, 255 us | 580 us, 1.0 ms |
| `advance_time` per frame, `answer()` with 8.6 signals | 48 ns, 3.2 us | 200 ns, 13 us |
| Controller start-up with a full save | 13.9 us | 56 us |
| Save file: write, read, `persist()` | 1.6 ms, 124 us, 2.1 ms | 6.6 ms, 0.5 ms, 8.3 ms |
| To first frame: Title (cold engine), Play on a phone, Level clear, Name entry, Top 10, About | 24, 16, 7, 12, 12, 23 ms | four times each |
| To first frame: Play on a tablet, on a desktop | 21 ms, 29 ms | 84 ms, 120 ms |
| Play frame time p95: phone, tablet, desktop | 2.0, 3.3, 3.5 ms | 8.1, 14, 14 ms |
| Play frames over 16.7 ms, of 600 per size | 0, 0, 3 (one of 54 ms, a one-off) | reported only |
| 100 screen cycles: new objects, process growth | 0, about 1.2 MB | 0, 4 MiB |
| GPU time of a Play frame (RTX 4090, Direct3D 11) | 0.03 ms median, 0.08 ms p95 | reported only |

The rules allocate nothing in a tick. An answer makes two allocations (the events it hands over) and dealing a question
about one (the deck, or the list of questions already dealt); the standard library of a Linux build counts a little fewer.
