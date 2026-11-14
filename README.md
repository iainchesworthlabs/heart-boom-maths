# heart-boom-maths

[![CI](https://github.com/iainchesworthlabs/heart-boom-maths/actions/workflows/ci.yml/badge.svg?branch=develop)](https://github.com/iainchesworthlabs/heart-boom-maths/actions/workflows/ci.yml)

A mental-arithmetic game for ages 5 to 12: the player answers ten number sentences a level against a heart-shaped bomb
timer. One C++23 core (`libs/core`, standard library only) holds the rules, and a Qt 6 and QML application
(`apps/game`) shows them on iPhone, iPad, Android, WebAssembly, macOS, Windows and Linux. It builds with CMake presets
and vcpkg, tests with Catch2 and CTest, and uses the shared CI, release and repository settings in
[icl-common-infra](https://github.com/iainchesworthlabs/icl-common-infra).

```bash
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
```

The Qt application is built when a Qt 6 kit is found (`ICL_QT_ROOT`); without one only the core is built and tested.
Documentation is in `docs/`. Start with `docs/SPEC.md` for the rules and `docs/building.md` for the build.

Licence: GPL-3.0-or-later. Contributions need a DCO sign-off (`git commit -s`).
