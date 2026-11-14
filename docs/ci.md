# Continuous integration

The workflows in `.github/workflows/` call reusable workflows in `icl-common-infra`, pinned by
commit SHA. Dependabot proposes the SHA bump; the pull request also needs
`tools/ci/sync_cmake.py` run, because the CMake modules are vendored.

## What runs

| Event | Jobs |
|---|---|
| Draft pull request | Policy, Plan, Project rules, workflow analysis, dependency scan |
| Ready pull request | Above, plus Linux release and sanitizer builds and coverage |
| Merge queue, push to `develop` | Above, plus Linux debug and TSan, macOS, Windows release and ASan |
| Change to what the iOS build reads (a path filter), or a manual run | iOS compile checks (`ios-build.yml` for the stub, `ios-game-build.yml` for the game; not required) |
| Change to what the Android build reads (a path filter), or a manual run | Android compile check (`android-build.yml`, not required) |
| Change to what the browser build reads (a path filter), or a manual run | WebAssembly build (`wasm-build.yml`, not required) |
| Nightly | CodeQL (`codeql.yml`), `nightly.yml` (sanitizers, static analysis and the dependency audit, below), and the performance tests (`perf.yml`, not required; also by hand) |
| Tag `v*` | Release |

The Plan job reads `.github/ci/plan.json`. A change to documentation only builds nothing. A path
the plan does not recognise builds everything.

## Nightly

`nightly.yml` runs at 02:37 UTC (and by hand: `gh workflow run nightly.yml --ref <branch>`, with `-f jobs=clang-tidy,cppcheck` to run
only some groups) against `develop`. No ruleset names it, so it cannot block a merge. Its jobs choose runners the way the
pull request gate does (`CI_RUNNER_MODE`, above). One run at a time: a night that is still going finishes before the
next starts. `docs/testing.md`, "Nightly checks", says what each job catches, how to run it by hand and how to read a failure.

| Job | What it runs | Runner | About |
|---|---|---|---|
| Sanitizer (Clang ASan -O1) | Every test label but `perf` and `fuzz`, `ui` on the offscreen platform, under AddressSanitizer and LeakSanitizer (preset `linux-asan-o1`) | Linux | 20 to 30 min |
| Sanitizer (Clang UBSan) | The same under UndefinedBehaviorSanitizer alone, `-fno-sanitize-recover=all` (`linux-ubsan`) | Linux | 20 to 30 min |
| Sanitizer (Clang TSan) | The same under ThreadSanitizer (`linux-tsan`) | Linux | 20 to 30 min |
| Sanitizer (GCC ASan and UBSan) | The same under GCC's runtimes (`linux-asan-ubsan`, built with GCC) | Linux | 20 to 30 min |
| Sanitizer (MSVC ASan /O1) | The same under MSVC's AddressSanitizer (`windows-msvc-asan`) | Windows | 30 to 45 min |
| Sanitizer (Apple Clang ASan and UBSan) | The same with Apple Clang (`macos-asan-ubsan`) | macOS, always hosted | 20 to 30 min |
| clang-tidy | `.clang-tidy`'s curated checks over `libs/core` and `apps/game`, counted against the baseline | Linux | 15 to 25 min |
| cppcheck | `warning`, `performance` and `portability` checks, counted against the baseline | Linux | 2 to 5 min |
| GCC analyzer | `libs/core` built with `-fanalyzer`, counted against the baseline | Linux | 5 to 10 min |
| MSVC analyze | Whole project built with `/analyze` and the C++ Core Guidelines checker (preset `windows-msvc-analyze`), counted against the baseline; no tests | Windows | 30 to 45 min |
| Dependencies | vcpkg port licences against GPL-3.0-or-later (fails), `pip-audit` of `requirements/docs.txt` (fails), freshness of the vcpkg baseline, ports, Qt, CMake and the NDK (informs) | Linux | 3 to 5 min |
| Nightly status | One table of every job and leg with its time and a link, the standing `nightly` issue (`nightly-report` of `icl-common-infra`), and a red run when any job is red | Hosted Linux | 1 min |

The durations are estimates from local runs and have not yet been measured on the runners. The
static analysers write SARIF to code scanning (`security-events: write`, in those jobs only) and keep
their logs for 14 days as artifacts. The sanitizer options are the `env` block at the top of the workflow.

CodeQL (`codeql.yml`, 18:17 UTC) builds `linux-release` with the Qt kit of `.github/toolchain-versions.json` and analyses
`libs/core` and `apps/game`, tests included. It does not see what the Linux build leaves out: the Qt Multimedia sound
backend (the reusable workflow installs Qt with no add-on modules), and the Windows, macOS, iOS, Android and browser
platform files. MSVC `/analyze` covers the Windows files.

## Runners

Where the jobs run is one setting, the `CI_RUNNER_MODE` repository variable. It is read fresh on every run.

| Mode | Linux and Windows jobs run on | Labels |
|---|---|---|
| unset or `hosted` | GitHub-hosted images (the default) | `ubuntu-26.04`, `windows-2025` |
| `self-hosted` | The organisation's on-premises fleet (runner group *Labs CI Runners*) | `self-hosted`, `Linux` or `Windows`, `X64`, `shared` |
| `overflow` | The same fleet, plus AWS spot instances that start when the fleet is busy (runner group *ci-aws-overflow*) | `self-hosted`, `Linux` or `Windows`, `X64` |

macOS legs, the zizmor job and **CI Status** always stay hosted. CI Status must report even when a fleet is down.

```bash
gh variable set CI_RUNNER_MODE --body overflow     # on: self-hosted or overflow
gh variable delete CI_RUNNER_MODE                  # off: back to hosted
gh workflow run ci.yml --ref <branch> -f runner_mode=overflow   # one run only, the variable untouched
```

- **Pull requests from forks always run hosted**, whatever the variable says. Fork code must not reach a runner inside
  the owner's network, and this repository is public.
- **The difference between `self-hosted` and `overflow` is the `shared` label.** The AWS dispatcher launches a spot
  instance only for a job whose labels are exactly `self-hosted, Linux, X64` (or the Windows equivalent). A job that also
  carries `shared` is never sent to AWS.
- **`overflow` needs two grants outside this repository.** The repository must be in the *ci-aws-overflow* runner group
  (done), and listed in `repository_white_list` in `ci-runners/terraform-aws/terraform.tfvars`, which takes a Terraform
  apply. Without the second, a job waits for the on-premises fleet only.
- **A dead fleet leaves jobs queued, not failed.** Delete the variable and re-run them.
- **Faster off switches for AWS only:** remove the repository from the *ci-aws-overflow* group (takes effect at once),
  or set `overflow_enabled = false` in `ci-runners` (needs an apply). The AWS budget also cuts launches off by itself at
  its monthly limit.
- **Try a mode with `workflow_dispatch` before setting the variable.** The shared build workflow has not been run on the
  self-hosted images with this project's Qt and MSVC pins.

## Required checks

Two jobs defined in `ci.yml` are the required checks: **CI Status** and **Scan dependency diff**.
CI Status fails when any job failed or was cancelled, and when a job the plan asked for was
skipped. Do not require checks from the called workflows by name; their names carry the caller's
job name as a prefix.

`ios-build.yml` is a separate workflow because a macOS runner is billed at a multiple and a required check that a path
filter keeps from running never reports. It compiles `apps/ios` unsigned with the `ios-release` preset and looks inside
the bundle; it needs no secret. The signed build and the TestFlight upload are the `ios` job of `release.yml`
(`docs/releasing.md`).

`ios-game-build.yml` is the same kind of workflow for the whole game: it installs the macOS and iOS Qt kits, configures the
`ios-game-release` preset, builds unsigned, and checks the bundle with `apps/game/ios/verify_game_app.sh` (orientations,
capabilities, Game Center entitlement file, frameworks, size in the job summary). It keeps its logs for a week when it
fails. See [iOS and Game Center](ios.md).
`android-build.yml` is separate for the same reason: the Qt kits are a download of several gigabytes. It builds the
`android-arm64-release` and `android-arm64-debug` presets on a Linux runner with the SDK, NDK and JDK that
`.github/toolchain-versions.json` pins, and `apps/game/android/verify_package.sh` fails it when a package asks for any
permission, is debuggable in the release, or carries more than `arm64-v8a`. It needs no secret and runs nothing on a
device. The signed build is the `android` job of `release.yml`, which runs only when the repository variable
`SIGN_ANDROID_UPLOAD` is `true` (`docs/releasing.md`). What a device has to show is listed in `docs/android.md`.
`perf.yml` runs the performance tests (label `perf`, [Performance tests](testing.md#performance-tests)) on the hosted Linux
release build once a night and on `workflow_dispatch`. It is not a required check and never runs on a pull request or the
merge queue, and it stays on the hosted runner class whatever `CI_RUNNER_MODE` says so that one night compares with the next.
The job summary holds the table of every test against its budget and the artifact `perf-results-linux-release` the
per-test JSON, `summary.json` and `summary.csv`. The jobs of `ci.yml` never build these tests (the `*-perf` presets do), so
the shared test runner's optional `perf` phase finds nothing to run there.
`wasm-build.yml` is separate for the same reason. Its three jobs build the rules for Node and compare eight games with
the native build, build the game for the browser and check the site and its size against a budget
(`WASM_GZIP_BUDGET_BYTES`, a placeholder to confirm), and open the site in Chrome. It installs two Emscripten releases
(the repository pin for the rules, the one Qt's kit demands for the game) and two Qt kits (desktop for the host tools,
WebAssembly). `docs/web.md` has the detail. The Project rules job of `ci.yml` also checks that the page template
reaches nothing outside itself.

## Run the checks locally

```bash
python3 tools/ci/precheck.py
```

It needs a checkout of `icl-common-infra` next to this repository or in `ICL_COMMON_INFRA`.
