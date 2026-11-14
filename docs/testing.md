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
| ui | `apps/game` (in `CMakeLists.txt`) | `game.smoke` starts the program on the offscreen platform and waits for the first frame; `game.qmllint` |

The `game.*` tests exist only when a Qt kit was found. On Windows the Qt `bin` directory must be on `PATH`;
`tools/dev/test.ps1` adds the `bin` directory of `ICL_QT_ROOT`. The scripts in `tools/ci/` carry their own tests: run
`python3 tools/ci/check_core_includes.py --self-test` and the same for `check_link_open.py`.

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
