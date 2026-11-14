# Building

## Prerequisites

CMake 4.0 or later, Ninja, and a C++23 compiler with `std::expected` and the C++23 library. CI builds with the
floor compilers, installed from the pins in `.github/toolchain-versions.json`: GCC 16, Clang 23 (with GCC 16's
libstdc++), MSVC 14.51 (a newer toolset passes) and the newest Apple Clang the macOS runners offer (Xcode 26.6 on
`macos-26`, with Xcode 27 tried in a leg that cannot fail the run). Older compilers may work but are not tested.

## Dependencies

Dependencies come from vcpkg, using the `iainchesworthlabs/vcpkg` fork. The build finds the fork, and Qt for a
project that uses it, through environment variables with the `ICL_` prefix, which are shared by every project of
the organisation; a variable with the prefix `HEART_BOOM_MATHS_` overrides it for this project. The presets do not
hard-code paths.

| Variable | Meaning |
|---|---|
| `ICL_VCPKG_STORE` | A directory for vcpkg checkouts. The commit named by `builtin-baseline` in `vcpkg.json` gets its own checkout in it (`<store>/<first 12 characters>`), made once and reused by every project that pins the same commit. |
| `ICL_VCPKG_MIRROR` | A clone of the fork to make those checkouts from. Default `<store>/mirror`, cloned on first use. |
| `ICL_QT_ROOT` | A prebuilt Qt kit, for example `~/Qt/6.12.0/gcc_64`. `QT_ROOT_DIR` and `QTDIR` are the fallbacks. Qt is not a vcpkg dependency; install it with aqtinstall. |
| `HEART_BOOM_MATHS_VCPKG_ROOT`, `HEART_BOOM_MATHS_QT_ROOT` | A vcpkg checkout or Qt kit used for this project only. |

A configure with none of them set stops and lists the options. Set them once in the shell profile. A project with a
Qt application pins the version in the `qt` key of `.github/toolchain-versions.json`; CI installs it from there.

## Presets

```bash
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
```

| Preset | Purpose |
|---|---|
| `linux-debug` | Debug with Tracy available (feature `profiling`) |
| `linux-release` | Optimised build |
| `linux-asan-ubsan` | AddressSanitizer and UndefinedBehaviorSanitizer |
| `linux-tsan` | ThreadSanitizer |
| `linux-coverage` | Instrumented build for gcovr |
| `macos-release`, `windows-msvc-release`, `windows-msvc-asan` | Platform builds |
| `ios-release` | The iPhone and iPad application (`apps/ios`), unsigned, on a Mac with Xcode; no tests and no Qt application |
| `wasm-release` | The game in a browser (`apps/game`), single-threaded. See WebAssembly below |
| `wasm-core` | The rules and the text harness for Node, to check the golden replays on WebAssembly. See WebAssembly below |

On Windows, run `pwsh -NoProfile -File tools/dev/test.ps1` to load the Visual Studio environment, build
`windows-msvc-release` and run the tests (`-Filter <regex>` runs some of them).

### The game

`apps/game` is a Qt 6 Quick application, built when a Qt 6.8 or newer kit is found (CI uses the version pinned in the
`qt` key of the toolchain file). The option `HEART_BOOM_MATHS_BUILD_GAME` has three values: `AUTO` (the default) builds
it when a kit is found and prints a status line when not, `ON` stops the configure if there is no kit, and `OFF` skips
it. A machine with no Qt still builds and tests `libs/core`. Set `ICL_QT_ROOT` once per machine (see above). The
program answers `--version` and, for tests, `--smoke-test` (it exits after the first frame).

Sound needs the `qtmultimedia` module of the Qt kit, which is an add-on in aqtinstall (`--modules qtmultimedia`) and a
selectable component of the official installer. The configure looks for it and, when it is missing, prints a
status line and builds a game that starts silent. See [Audio](audio.md).

The build information (version, build number, short commit, branch or tag, date, Qt version) is written at configure
time by `cmake/HbmBuildInfo.cmake`. The build number is `ICL_BUILD_NUMBER`, then `GITHUB_RUN_NUMBER`, and 0 when
neither is a whole number. A commit made after the configure shows at the next configure. The notices text About
shows is built at the same time by `cmake/HbmNotices.cmake`: Qt, each runtime dependency of `vcpkg.json` (the configure
stops when one has no licence file, so list a test-only package in `EXCLUDE`) and the two font licences.

### WebAssembly

`wasm-release` builds `apps/game` for a browser with Qt's single-threaded kit, so the page needs no cross-origin isolation
headers. The preset is listed only when `EMSDK` is set. It needs three things in the environment:

| Variable | Meaning |
|---|---|
| `EMSDK` | An activated emsdk of the Emscripten release the Qt kit was built with: **5.0.5 for Qt 6.12.0**. The kit stops the configure with any other. (The `emscripten` key of `.github/toolchain-versions.json`, 6.0.6, is for builds without Qt, such as `wasm-core` below.) |
| `HEART_BOOM_MATHS_QT_WASM_ROOT` | Qt's `wasm_singlethread` kit of the pinned Qt version. With aqtinstall: `aqt install-qt all_os wasm 6.12.0 wasm_singlethread` (the kit is under the host `all_os` and the target `wasm`) |
| `ICL_QT_ROOT` | The desktop kit of the same Qt version, which Qt uses for its host tools |

```bash
cmake --preset wasm-release
cmake --build --preset wasm-release
python3 -m http.server --directory build/wasm-release/site 8000   # then open http://localhost:8000
```

The result is the folder `build/wasm-release/site`: the page, its script and style, Qt's loader, the module and the
licence and notices, with nothing else, ready for any static host. `docs/web.md` describes the page, the save, the size
and the checks. No tests are built for it and vcpkg is not used. `python3 tools/ci/check_web_site.py --site
build/wasm-release/site` checks the site and reports its size.

`wasm-core` builds only `libs/core` and the text harness with a plain Emscripten (the `emscripten` pin, no Qt), to run under
Node:

```bash
cmake --preset wasm-core && cmake --build --preset wasm-core
node build/wasm-core/libs/core/heart_boom_maths_harness.js --check libs/core/tests/data/replays
```

The harness is built without the tests when `HEART_BOOM_MATHS_BUILD_HARNESS=ON` and `HEART_BOOM_MATHS_BUILD_TESTS=OFF`.

Sanitizers, Tracy, coverage and fuzzing exclude each other. The configure step stops with a
message when two are enabled.

### iOS

`cmake --preset ios-release` needs a vcpkg checkout at the baseline of `vcpkg.json` (`VCPKG_ROOT`; CI's `setup-vcpkg` action
sets it) and Xcode. It builds `build/ios-release/heart-boom-maths.app` without signing. Three cache variables make a signed
build, which is the release workflow's (`docs/releasing.md`, "iOS and TestFlight"): `HEART_BOOM_MATHS_IOS_SIGNING_TEAM` and
`HEART_BOOM_MATHS_IOS_SIGNING_PROFILE` go together (CMake stops when only one is given), and `HEART_BOOM_MATHS_IOS_BUILD_NUMBER`
sets `CFBundleVersion`. The identifier is `HEART_BOOM_MATHS_APPLICATION_ID` in `CMakeLists.txt`. Check the bundle with
`apps/ios/packaging/verify_app.sh <app> --bundle-id <id> --executable heart-boom-maths`.

Put personal settings in `CMakeUserPresets.json`, which is ignored by git.
