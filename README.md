# heart-boom-maths

[![CI](https://github.com/iainchesworthlabs/heart-boom-maths/actions/workflows/ci.yml/badge.svg?branch=develop)](https://github.com/iainchesworthlabs/heart-boom-maths/actions/workflows/ci.yml)

Cross-platform app and game for iOS, Android, WebAssembly and desktop. It builds with CMake presets and vcpkg, tests with Catch2 and CTest, and uses the shared
CI, release and repository settings in
[icl-common-infra](https://github.com/iainchesworthlabs/icl-common-infra).

```bash
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
```

Documentation is in `docs/`.

Licence: GPL-3.0-or-later. Contributions need a DCO sign-off (`git commit -s`).
