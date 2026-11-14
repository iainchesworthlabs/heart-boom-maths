#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Keep Tracy out of the builds that must not have it.

    python3 tools/ci/check_presets_tracy.py [--root DIR]
    python3 tools/ci/check_presets_tracy.py --self-test

The desktop release presets carry the Tracy client, dormant (docs/profiling.md). Tracy opens a listening socket when it is
started, so the builds that ship as web pages and phone apps, and the sanitizer, coverage and fuzz builds, must not have it
at all. For every configure preset in CMakePresets.json, with `inherits` resolved, this reports a preset that:

  mobile-web   builds for WebAssembly, Android or iOS (by its toolchain, its system name or its name) and has
               ICL_ENABLE_TRACY on or the vcpkg feature `profiling` selected
  instrumented has a sanitizer (ICL_SANITIZERS), coverage (ICL_ENABLE_COVERAGE) or fuzzing (ICL_FUZZING) on and has
               ICL_ENABLE_TRACY on or the feature `profiling` selected

The CMake configure refuses the same combinations (cmake/HbmProfiling.cmake, cmake/icl/IclSanitizers.cmake); this reads the
presets, so a wrong one is found without configuring it. Exit status 0 when clean, 1 on a finding, 2 on a usage error.
"""

from __future__ import annotations

import argparse
import json
import sys
import tempfile
from pathlib import Path

MOBILE_WEB_NAMES = ("ios", "android", "wasm")
OFF_VALUES = {"", "OFF", "FALSE", "0", "NO", "N", "IGNORE", "NOTFOUND"}


def resolved(presets: dict[str, dict], name: str, seen: tuple[str, ...] = ()) -> dict:
    """The preset's cache variables and toolchain, with its parents' under it (the first parent wins)."""
    preset = presets[name]
    parents = preset.get("inherits", [])
    if isinstance(parents, str):
        parents = [parents]
    cache: dict[str, str] = {}
    toolchain = ""
    for parent in reversed(parents):
        if parent in seen or parent not in presets:
            continue
        inherited = resolved(presets, parent, (*seen, name))
        cache.update(inherited["cache"])
        toolchain = inherited["toolchain"] or toolchain
    for key, value in preset.get("cacheVariables", {}).items():
        cache[key] = value["value"] if isinstance(value, dict) else value
    return {"cache": {k: str(v) for k, v in cache.items()}, "toolchain": preset.get("toolchainFile", toolchain)}


def is_on(value: str | None) -> bool:
    return value is not None and value.strip().upper() not in OFF_VALUES


def has_profiling_feature(cache: dict[str, str]) -> bool:
    features = [f.strip() for f in cache.get("VCPKG_MANIFEST_FEATURES", "").split(";")]
    return "profiling" in features


def find_problems(root: Path) -> list[str]:
    path = root / "CMakePresets.json"
    if not path.is_file():
        return [f"{path}: not found"]
    data = json.loads(path.read_text(encoding="utf-8"))
    presets = {p["name"]: p for p in data.get("configurePresets", [])}
    problems: list[str] = []
    for name in presets:
        info = resolved(presets, name)
        cache = info["cache"]
        has_tracy = is_on(cache.get("ICL_ENABLE_TRACY")) or has_profiling_feature(cache)
        if not has_tracy:
            continue
        toolchain = info["toolchain"].lower()
        system = cache.get("CMAKE_SYSTEM_NAME", "").lower()
        if name.startswith(MOBILE_WEB_NAMES) or "emscripten" in toolchain or "qt.toolchain" in toolchain \
                or system in {"ios", "android"}:
            problems.append(f"{name}: mobile-web: a WebAssembly, Android or iOS preset has Tracy on")
        if is_on(cache.get("ICL_SANITIZERS")) or is_on(cache.get("ICL_ENABLE_COVERAGE")) or is_on(cache.get("ICL_FUZZING")):
            problems.append(f"{name}: instrumented: a sanitizer, coverage or fuzzing preset has Tracy on")
    return problems


def self_test() -> int:
    def run(presets: list[dict]) -> list[str]:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "CMakePresets.json").write_text(json.dumps({"version": 8, "configurePresets": presets}))
            return find_problems(root)

    base = {"name": "base", "hidden": True}
    results = []

    def expect(label: str, presets: list[dict], *kinds: str) -> None:
        problems = run(presets)
        found = sorted(k for k in ("mobile-web", "instrumented") if any(f": {k}:" in p for p in problems))
        ok = found == sorted(kinds)
        print(f"{'ok  ' if ok else 'FAIL'} {label}" + ("" if ok else f"  expected {list(kinds)}, got {problems}"))
        results.append(ok)

    on = {"ICL_ENABLE_TRACY": "ON", "VCPKG_MANIFEST_FEATURES": "profiling"}
    expect("a desktop release with Tracy", [base, {"name": "linux-release", "inherits": "base", "cacheVariables": on}])
    expect("a preset with no Tracy", [base, {"name": "ios-release", "inherits": "base", "cacheVariables": {}}])
    expect("an iOS preset with Tracy", [base, {"name": "ios-release", "inherits": "base", "cacheVariables": on}],
           "mobile-web")
    expect("Tracy inherited by an Android preset",
           [{"name": "t", "hidden": True, "cacheVariables": on}, {"name": "android-arm64", "inherits": "t"}], "mobile-web")
    expect("a WebAssembly preset by its toolchain",
           [base, {"name": "web", "inherits": "base", "toolchainFile": "x/Emscripten.cmake", "cacheVariables": on}],
           "mobile-web")
    expect("a sanitizer preset with Tracy",
           [base, {"name": "asan", "inherits": "base", "cacheVariables": {**on, "ICL_SANITIZERS": "address"}}],
           "instrumented")
    expect("a sanitizer preset that turns it off again",
           [{"name": "rel", "hidden": True, "cacheVariables": on},
            {"name": "asan", "inherits": "rel",
             "cacheVariables": {"ICL_SANITIZERS": "address", "ICL_ENABLE_TRACY": "OFF", "VCPKG_MANIFEST_FEATURES": ""}}])
    expect("a coverage preset with Tracy",
           [base, {"name": "cov", "inherits": "base", "cacheVariables": {**on, "ICL_ENABLE_COVERAGE": "ON"}}],
           "instrumented")
    missing = find_problems(Path(tempfile.gettempdir()) / "check-presets-tracy-no-such-root")
    results.append(bool(missing))
    print(f"{'ok  ' if missing else 'FAIL'} missing CMakePresets.json is reported")
    return 0 if all(results) else 1


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--root", default=".", help="repository root (default: the current directory)")
    parser.add_argument("--self-test", action="store_true", help="run the checker's own tests and exit")
    args = parser.parse_args(argv)
    if args.self_test:
        return self_test()
    problems = find_problems(Path(args.root).resolve())
    for problem in problems:
        print(problem, file=sys.stderr)
    if problems:
        print(f"{len(problems)} problem(s): Tracy must not be in a mobile, web, sanitizer, coverage or fuzz preset.",
              file=sys.stderr)
        return 1
    print("No WebAssembly, Android, iOS, sanitizer, coverage or fuzz preset has Tracy.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
