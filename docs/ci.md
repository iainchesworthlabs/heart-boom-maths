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
| Change to what the iOS build reads (a path filter), or a manual run | iOS compile check (`ios-build.yml`, not required) |
| Change to what the browser build reads (a path filter), or a manual run | WebAssembly build (`wasm-build.yml`, not required) |
| Nightly | CodeQL |
| Tag `v*` | Release |

The Plan job reads `.github/ci/plan.json`. A change to documentation only builds nothing. A path
the plan does not recognise builds everything.

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
