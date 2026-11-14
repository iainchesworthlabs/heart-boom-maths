#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""The nightly's look at what the project depends on: licences (fails) and freshness (only informs).

    python3 tools/ci/dependency_audit.py licences --vcpkg-root DIR [--summary FILE]
    python3 tools/ci/dependency_audit.py freshness [--vcpkg-root DIR] [--summary FILE]
    python3 tools/ci/dependency_audit.py --self-test

licences  Resolves every vcpkg port that vcpkg.json asks for, with the features it names, down through the ports' own
          dependencies, in the ports directory of the vcpkg checkout at the manifest's baseline (what setup-vcpkg
          makes). It prints each port's version and SPDX licence expression, and fails when a licence is missing or is
          not one that may be combined with GPL-3.0-or-later. Host-only helper ports (vcpkg-cmake and the like) are
          listed too: they are not linked, but a licence nobody read is not a reason to leave them out.
freshness Compares the vcpkg baseline, the ports in use, Qt, the Android NDK and CMake with the newest release, for the
          job summary. It never fails: a network error is a line saying so.

A vulnerability scan of vcpkg ports is not here because no database covers them: the OSV database has no vcpkg
ecosystem (osv-scanner has no extractor for vcpkg.json, and a query by package URL answers nothing). Dependabot's vcpkg
ecosystem and its security updates are what flag a port; the Python requirements are scanned with pip-audit in the
workflow.

Exit status 0 when every licence is acceptable, 1 when one is not, 2 on a usage error.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
import tempfile
import urllib.request
from pathlib import Path

# SPDX identifiers that may be combined with GPL-3.0-or-later in one program, as the Free Software Foundation lists
# them for GPLv3 (permissive licences, the Apache licence 2.0, the MPL 2.0, and the GPL and LGPL themselves). The
# project's own licence is GPL-3.0-or-later (CLAUDE.md), so GPL-3.0-only code would also fit; the AGPL and any
# "non-commercial" or "source-available" licence would not.
ACCEPTABLE = frozenset(
    """
    0BSD Apache-2.0 BSD-1-Clause BSD-2-Clause BSD-3-Clause BSL-1.0 CC0-1.0 FTL GPL-3.0-only GPL-3.0-or-later
    ISC LGPL-2.1-only LGPL-2.1-or-later LGPL-3.0-only LGPL-3.0-or-later MIT MIT-0 MPL-2.0 NCSA OFL-1.1 Unlicense Zlib
    libpng-2.0 curl X11 Libpng IJG ICU bzip2-1.0.6 blessing MIT-open-group X11-distribute-modifications-variant
    OLDAP-2.8 Python-2.0 GPL-2.0-or-later
    """.split()
)
# Exceptions to a licence (the part after WITH) that only widen what it allows.
ACCEPTABLE_EXCEPTIONS = frozenset({"LLVM-exception", "GCC-exception-3.1", "Classpath-exception-2.0", "Qt-GPL-exception-1.0"})


def licence_ok(expression: str) -> tuple[bool, list[str]]:
    """Whether an SPDX expression can be satisfied with acceptable licences, and the identifiers that stopped it."""
    tokens = re.findall(r"\(|\)|[A-Za-z0-9.+\-]+", expression)
    pos = 0
    bad: list[str] = []

    def atom() -> bool:
        nonlocal pos
        tok = tokens[pos]
        if tok == "(":
            pos += 1
            ok = expr()
            pos += 1  # the closing parenthesis
            return ok
        pos += 1
        name = tok.rstrip("+")
        ok = name in ACCEPTABLE or tok in ACCEPTABLE
        if pos < len(tokens) and tokens[pos] == "WITH":
            pos += 1
            exception = tokens[pos]
            pos += 1
            if exception not in ACCEPTABLE_EXCEPTIONS:
                ok = False
                bad.append(f"{tok} WITH {exception}")
                return False
        if not ok:
            bad.append(tok)
        return ok

    def conj() -> bool:
        nonlocal pos
        ok = atom()
        while pos < len(tokens) and tokens[pos] == "AND":
            pos += 1
            ok = atom() and ok
        return ok

    def expr() -> bool:
        nonlocal pos
        ok = conj()
        while pos < len(tokens) and tokens[pos] == "OR":
            pos += 1
            ok = conj() or ok
        return ok

    if not tokens:
        return False, ["(none)"]
    result = expr()
    return result, ([] if result else bad)


def read_port(ports: Path, name: str) -> dict | None:
    path = ports / name / "vcpkg.json"
    if not path.is_file():
        return None
    return json.loads(path.read_text(encoding="utf-8"))


def port_version(port: dict) -> str:
    for key in ("version", "version-semver", "version-date", "version-string"):
        if key in port:
            return f"{port[key]}#{port['port-version']}" if port.get("port-version") else str(port[key])
    return "?"


def resolve(manifest: dict, ports: Path, features: tuple[str, ...] = ()) -> dict[str, dict]:
    """Every port the manifest reaches: its dependencies, the ports' own dependencies and default features, and the
    dependencies of the manifest's features named in `features` (all of them when empty, so the audit sees the
    superset a CI leg with `profiling` builds)."""
    requested: dict[str, set[str]] = {}
    use_defaults: dict[str, bool] = {}
    queue: list[str] = []

    def want(entry, names: set[str] | None = None) -> None:
        if isinstance(entry, str):
            name, wanted_features, defaults = entry, set(), True
        else:
            name, wanted_features, defaults = entry["name"], set(entry.get("features", [])), entry.get("default-features", True)
        known = requested.setdefault(name, set())
        grew = not wanted_features <= known or name not in use_defaults
        known |= wanted_features
        use_defaults[name] = use_defaults.get(name, False) or defaults
        if grew:
            queue.append(name)

    for entry in manifest.get("dependencies", []):
        want(entry)
    for key, feature in manifest.get("features", {}).items():
        if not features or key in features:
            for entry in feature.get("dependencies", []):
                want(entry)
    found: dict[str, dict] = {}
    while queue:
        name = queue.pop()
        port = read_port(ports, name)
        if port is None:
            found[name] = {"name": name, "missing": True}
            continue
        found[name] = port
        enabled = set(requested[name])
        if use_defaults[name]:
            enabled |= set(port.get("default-features", []))
        for entry in port.get("dependencies", []):
            want(entry)
        for key in sorted(enabled):
            feature = port.get("features", {}).get(key, {})
            for entry in feature.get("dependencies", []):
                want(entry)
    return found


def licences(args: argparse.Namespace) -> int:
    manifest = json.loads(Path(args.manifest).read_text(encoding="utf-8"))
    found = resolve(manifest, Path(args.vcpkg_root) / "ports")
    rows = ["| Port | Version | Licence | Result |", "|---|---|---|---|"]
    failures = []
    for name in sorted(found):
        port = found[name]
        if port.get("missing"):
            rows.append(f"| {name} | | | **no such port in this vcpkg checkout** |")
            failures.append(name)
            continue
        expression = port.get("license")
        ok, bad = (False, ["(none)"]) if not expression else licence_ok(expression)
        rows.append(f"| {name} | {port_version(port)} | {expression or '(none)'} | {'ok' if ok else '**' + ', '.join(bad) + '**'} |")
        if not ok:
            failures.append(name)
    text = "### vcpkg licences (must fit GPL-3.0-or-later)\n\n" + "\n".join(rows) + "\n"
    if failures:
        text += "\nNot acceptable: " + ", ".join(failures) + ". Replace the port, or add the licence to ACCEPTABLE in tools/ci/dependency_audit.py after reading it.\n"
    print(text)
    if args.summary:
        with open(args.summary, "a", encoding="utf-8") as out:
            out.write(text + "\n")
    return 1 if failures else 0


# ---------------------------------------------------------------------------------------------------------------------
# Freshness


def fetch(url: str) -> str:
    headers = {"User-Agent": "dependency-audit", "Accept": "application/vnd.github+json"}
    if "github.com" in url and os.environ.get("GITHUB_TOKEN"):
        headers["Authorization"] = "Bearer " + os.environ["GITHUB_TOKEN"]  # a public API, but the anonymous limit is low
    request = urllib.request.Request(url, headers=headers)
    with urllib.request.urlopen(request, timeout=30) as response:  # noqa: S310 - fixed https URLs
        return response.read().decode("utf-8")


def version_key(text: str) -> tuple:
    return tuple(int(p) for p in re.findall(r"\d+", text)[:4])


def newest_qt() -> str:
    out = subprocess.run(["git", "ls-remote", "--tags", "https://github.com/qt/qtbase.git", "v6.*"], check=True, capture_output=True, text=True, timeout=60).stdout
    versions = {m.group(1) for m in re.finditer(r"refs/tags/v(6\.\d+\.\d+)$", out, re.MULTILINE)}
    return max(versions, key=version_key)


def newest_ndk() -> str:
    tag = json.loads(fetch("https://api.github.com/repos/android/ndk/releases/latest"))["tag_name"]
    return tag


def newest_cmake() -> str:
    return json.loads(fetch("https://api.github.com/repos/Kitware/CMake/releases/latest"))["tag_name"].lstrip("v")


def master_port_version(name: str) -> str:
    raw = fetch(f"https://raw.githubusercontent.com/microsoft/vcpkg/master/ports/{name}/vcpkg.json")
    return port_version(json.loads(raw))


def freshness(args: argparse.Namespace) -> int:
    pins = json.loads(Path(args.pins).read_text(encoding="utf-8"))
    manifest = json.loads(Path(args.manifest).read_text(encoding="utf-8"))
    rows = ["| What | Pinned | Newest | |", "|---|---|---|---|"]

    def row(what: str, pinned: str, getter) -> None:
        try:
            newest = getter()
            mark = "up to date" if version_key(newest) <= version_key(pinned) else "newer exists"
            rows.append(f"| {what} | {pinned} | {newest} | {mark} |")
        except Exception as error:  # noqa: BLE001 - informational, never fails the run
            rows.append(f"| {what} | {pinned} | (could not be read: {type(error).__name__}) | |")

    row("Qt", pins.get("qt", "?"), newest_qt)
    row("CMake", pins.get("cmake", "?"), newest_cmake)
    ndk = pins.get("android", {}).get("ndk", "?")
    row("Android NDK", ndk, lambda: _ndk_as_version(newest_ndk()))
    baseline = manifest.get("builtin-baseline", "?")
    try:
        head = json.loads(fetch("https://api.github.com/repos/microsoft/vcpkg/commits/master"))
        pinned_commit = json.loads(fetch(f"https://api.github.com/repos/microsoft/vcpkg/commits/{baseline}"))
        rows.append(
            f"| vcpkg baseline | {baseline[:12]} ({pinned_commit['commit']['committer']['date'][:10]}) "
            f"| {head['sha'][:12]} ({head['commit']['committer']['date'][:10]}) | |"
        )
    except Exception as error:  # noqa: BLE001
        rows.append(f"| vcpkg baseline | {baseline[:12]} | (could not be read: {type(error).__name__}) | |")
    if args.vcpkg_root:
        ports = Path(args.vcpkg_root) / "ports"
        for name in sorted(resolve(manifest, ports)):
            port = read_port(ports, name)
            if port is None or name.startswith("vcpkg-"):
                continue
            row(f"vcpkg port {name}", port_version(port), lambda n=name: master_port_version(n))
    text = "### Freshness (informational)\n\n" + "\n".join(rows) + "\n"
    print(text)
    if args.summary:
        with open(args.summary, "a", encoding="utf-8") as out:
            out.write(text + "\n")
    return 0


def _ndk_as_version(tag: str) -> str:
    """The GitHub tag is the release name (r28c); the pin is the full version (27.2.12479018). Show both as the tag."""
    return tag


def self_test() -> int:
    failures: list[str] = []

    def expect(name: str, ok: bool) -> None:
        if not ok:
            failures.append(name)

    expect("MIT", licence_ok("MIT")[0])
    expect("BSL-1.0 AND MIT", licence_ok("BSL-1.0 AND MIT")[0])
    expect("OR takes the acceptable one", licence_ok("MIT OR Proprietary")[0])
    expect("AND needs both", not licence_ok("MIT AND Proprietary")[0])
    expect("parentheses", licence_ok("(MIT OR GPL-2.0-only) AND Zlib")[0])
    expect("AGPL is out", not licence_ok("AGPL-3.0-or-later")[0])
    expect("GPL-3.0-or-later with a good exception", licence_ok("GPL-3.0-or-later WITH GCC-exception-3.1")[0])
    expect("unknown exception", not licence_ok("Apache-2.0 WITH Made-up-exception")[0])
    expect("empty", not licence_ok("")[0])
    expect("Apache-2.0 WITH LLVM-exception", licence_ok("Apache-2.0 WITH LLVM-exception")[0])
    with tempfile.TemporaryDirectory() as tmp:
        ports = Path(tmp)
        for name, body in {
            "a": {"name": "a", "version": "1", "license": "MIT", "dependencies": [{"name": "b", "host": True}]},
            "b": {"name": "b", "version-date": "2025-01-01", "license": "SSPL-1.0"},
        }.items():
            (ports / name).mkdir()
            (ports / name / "vcpkg.json").write_text(json.dumps(body), encoding="utf-8")
        found = resolve({"dependencies": ["a"]}, ports)
        expect("transitive ports are found", set(found) == {"a", "b"})
        expect("a bad transitive licence is seen", not licence_ok(found["b"]["license"])[0])
    if failures:
        print("dependency_audit self-test failed: " + "; ".join(failures))
        return 1
    print("dependency_audit self-test passed")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--self-test", action="store_true")
    sub = parser.add_subparsers(dest="command")
    for name in ("licences", "freshness"):
        p = sub.add_parser(name)
        p.add_argument("--manifest", default="vcpkg.json")
        p.add_argument("--vcpkg-root", required=name == "licences", help="a vcpkg checkout at the manifest's baseline")
        p.add_argument("--summary", help="append the Markdown to this file (GITHUB_STEP_SUMMARY)")
        if name == "freshness":
            p.add_argument("--pins", default=".github/toolchain-versions.json")
    args = parser.parse_args()
    if args.self_test:
        return self_test()
    if args.command == "licences":
        return licences(args)
    if args.command == "freshness":
        return freshness(args)
    parser.print_usage(sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main())
