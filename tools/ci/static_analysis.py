#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Count what a static analyser reported, compare it with the baseline, and write SARIF.

    python3 tools/ci/static_analysis.py check --tool clang-tidy --log clang-tidy.log [--sarif out.sarif]
    python3 tools/ci/static_analysis.py check --tool cppcheck --log cppcheck.log
    python3 tools/ci/static_analysis.py check --tool gcc-analyzer --log build.log
    python3 tools/ci/static_analysis.py check --tool msvc-analyze --log build.log
    python3 tools/ci/static_analysis.py lower --tool clang-tidy --log clang-tidy.log     # lower the baseline
    python3 tools/ci/static_analysis.py accept --tool clang-tidy --log clang-tidy.log    # make it exactly today's counts
    python3 tools/ci/static_analysis.py files --compile-commands build/linux-static-analysis/compile_commands.json
    python3 tools/ci/static_analysis.py --self-test

The tools differ in what they can write, so each one's console output is the input here and this script writes the one
SARIF file code scanning takes. A finding is counted under the key "<check> <path relative to the repository>", which does
not move when a line does. The baseline (tools/static-analysis-baseline.json) is those counts for the findings that are
accepted for now: `check` fails when a key has more findings than the baseline allows, or is not in it, and says so for
each. A count that fell is reported and does not fail; `lower` writes the new, smaller counts into the baseline and never
raises one. `accept` writes today's counts whatever they are: it is how a baseline starts and how a finding that cannot be
fixed is taken in, and the diff of the file is what a reviewer reads.

Only files of this repository count: a header of Qt, vcpkg or the compiler, and anything under a build directory (the
moc and rcc sources among them), is dropped. Findings in test code count like the rest.

Exit status 0 when nothing new was found, 1 when something was (or a build failed with no finding to show for it), 2 on a
usage error.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
import tempfile
from collections import Counter
from pathlib import Path

BASELINE = Path("tools/static-analysis-baseline.json")
TOOLS = ("clang-tidy", "cppcheck", "gcc-analyzer", "msvc-analyze")

# <file>:<line>:<column>: warning: <message> [<check>]   (clang-tidy, GCC, and cppcheck with --template)
GCC_STYLE = re.compile(
    r"^(?P<file>(?:[A-Za-z]:)?[^:\n]+?):(?P<line>\d+):(?:(?P<col>\d+):)?\s*(?P<level>warning|error):\s*"
    r"(?P<message>.*?)\s*\[(?P<check>[A-Za-z0-9_.\-,=]+)\]\s*$"
)
# <file>(<line>[,<col>]): warning C6001: <message>   (MSVC)
MSVC_STYLE = re.compile(
    r"^(?:\s*\d+>)?(?P<file>(?:[A-Za-z]:)?[^(\n]+?)\((?P<line>\d+)(?:,(?P<col>\d+))?\)\s*:\s*warning\s+(?P<check>C\d+)\s*:\s*"
    r"(?P<message>.*?)(?:\s*\[[^\]]*\])?\s*$"
)
# Paths that are never this repository's own sources.
GENERATED = re.compile(r"(^|/)(build|_deps|vcpkg_installed|\.icl-infra)/|(^|/)(moc_|qrc_|ui_)[^/]*$|\.moc$|/autogen")


def normalise(path: str, root: Path) -> str | None:
    """The path relative to the repository root with forward slashes, or None when the file is not ours."""
    path = path.strip().replace("\\", "/")
    root_text = str(root.resolve()).replace("\\", "/").rstrip("/")
    if path.lower().startswith(root_text.lower() + "/"):
        path = path[len(root_text) + 1 :]
    elif re.match(r"^(?:[A-Za-z]:)?/", path):
        return None  # absolute and outside the repository: the compiler's, Qt's or vcpkg's
    while path.startswith("./"):
        path = path[2:]
    if path.startswith("../") or GENERATED.search(path):
        return None
    return path


def parse(tool: str, text: str, root: Path) -> list[dict]:
    """Findings from a tool's console output, each {check, file, line, message}, without duplicates."""
    pattern = MSVC_STYLE if tool == "msvc-analyze" else GCC_STYLE
    seen: set[tuple] = set()
    findings: list[dict] = []
    for raw in text.splitlines():
        match = pattern.match(raw.rstrip())
        if not match:
            continue
        file = normalise(match["file"], root)
        if file is None:
            continue
        # GCC names a warning that -Werror made an error "-Werror=analyzer-..."; clang-tidy joins aliases with commas.
        check = match["check"].split(",")[0].removeprefix("-Werror=")
        if tool == "clang-tidy" and check.startswith("clang-diagnostic-") and match["level"] != "error":
            continue  # a compiler warning, which the build already reports
        # The same header is analysed once per translation unit that includes it; it is one finding.
        key = (check, file, int(match["line"]), match["message"])
        if key in seen:
            continue
        seen.add(key)
        findings.append({"check": check, "file": file, "line": int(match["line"]), "message": match["message"]})
    return findings


def count(findings: list[dict]) -> Counter:
    return Counter(f"{f['check']} {f['file']}" for f in findings)


def load_baseline(path: Path) -> dict:
    if not path.is_file():
        return {}
    data = json.loads(path.read_text(encoding="utf-8"))
    return data.get("tools", {})


def save_baseline(path: Path, tools: dict) -> None:
    body = {"tools": {t: dict(sorted(tools[t].items())) for t in sorted(tools)}}
    path.write_text(json.dumps(body, indent=2) + "\n", encoding="utf-8", newline="\n")


def compare(found: Counter, allowed: dict) -> tuple[list[tuple[str, int, int]], list[tuple[str, int, int]]]:
    """(new, fixed): the keys with more findings than allowed, and the keys with fewer."""
    new = [(k, allowed.get(k, 0), n) for k, n in sorted(found.items()) if n > allowed.get(k, 0)]
    fixed = [(k, n, found.get(k, 0)) for k, n in sorted(allowed.items()) if found.get(k, 0) < n]
    return new, fixed


def to_sarif(tool: str, findings: list[dict]) -> dict:
    rules = sorted({f["check"] for f in findings})
    return {
        "$schema": "https://json.schemastore.org/sarif-2.1.0.json",
        "version": "2.1.0",
        "runs": [
            {
                "tool": {"driver": {"name": tool, "rules": [{"id": r} for r in rules]}},
                "results": [
                    {
                        "ruleId": f["check"],
                        "level": "warning",
                        "message": {"text": f["message"]},
                        "locations": [
                            {
                                "physicalLocation": {
                                    "artifactLocation": {"uri": f["file"], "uriBaseId": "%SRCROOT%"},
                                    "region": {"startLine": max(f["line"], 1)},
                                }
                            }
                        ],
                    }
                    for f in findings
                ],
            }
        ],
    }


def read_logs(paths: list[str]) -> str:
    return "\n".join(Path(p).read_text(encoding="utf-8", errors="replace") for p in paths)


def summary_text(tool: str, findings: list[dict], new, fixed, baseline_total: int) -> str:
    lines = [f"### {tool}", "", f"{len(findings)} findings; the baseline accepts {baseline_total}.", ""]
    if new:
        lines += ["New findings (more than the baseline allows):", "", "| Check and file | Baseline | Now |", "|---|---|---|"]
        lines += [f"| `{k}` | {a} | {n} |" for k, a, n in new]
        lines.append("")
        for f in findings:
            if any(f"{f['check']} {f['file']}" == k for k, _, _ in new):
                lines.append(f"- `{f['file']}:{f['line']}` {f['message']} [{f['check']}]")
        lines.append("")
    if fixed:
        lines += [
            "The baseline is higher than the findings now. Lower it with "
            f"`python3 tools/ci/static_analysis.py lower --tool {tool} --log <log>` and commit the file:",
            "",
        ]
        lines += [f"- `{k}`: {a} accepted, {n} found" for k, a, n in fixed]
        lines.append("")
    return "\n".join(lines)


def run(args: argparse.Namespace) -> int:
    root = Path(args.root)
    findings = parse(args.tool, read_logs(args.log), root)
    found = count(findings)
    baseline_path = root / BASELINE
    tools = load_baseline(baseline_path)
    allowed = tools.get(args.tool, {})
    if args.sarif:
        Path(args.sarif).write_text(json.dumps(to_sarif(args.tool, findings), indent=1) + "\n", encoding="utf-8")
    new, fixed = compare(found, allowed)
    if args.command == "accept":
        tools[args.tool] = dict(found)
        save_baseline(baseline_path, tools)
        print(f"{args.tool}: baseline now {sum(found.values())} findings (was {sum(allowed.values())}).")
        return 0
    if args.command == "lower":
        lowered = {k: min(n, found.get(k, 0)) for k, n in allowed.items() if found.get(k, 0) > 0}
        tools[args.tool] = lowered
        save_baseline(baseline_path, tools)
        print(f"{args.tool}: baseline now {sum(lowered.values())} findings (was {sum(allowed.values())}).")
        if new:
            print(f"{len(new)} keys have more findings than accepted; they were not added. Fix them or accept them by hand.")
            return 1
        return 0
    text = summary_text(args.tool, findings, new, fixed, sum(allowed.values()))
    # A tool that ran inside a build says only "the build failed" when something other than a finding stopped it.
    broken = args.build_status not in (None, 0) and not findings
    if broken:
        text += (
            f"\nThe build exited with status {args.build_status} and no finding was read from its log, so the run "
            "says nothing about the code. Read the log.\n"
        )
    print(text)
    if args.summary:
        with open(args.summary, "a", encoding="utf-8") as out:
            out.write(text + "\n")
    return 1 if new or broken else 0


def list_files(args: argparse.Namespace) -> int:
    """The repository's own translation units in a compile_commands.json, one per line."""
    root = Path(args.root)
    entries = json.loads(Path(args.compile_commands).read_text(encoding="utf-8"))
    files = set()
    for entry in entries:
        path = Path(entry["file"])
        if not path.is_absolute():
            path = Path(entry["directory"]) / path
        relative = normalise(str(path), root)
        if relative is not None and relative.split("/")[0] in ("libs", "apps"):
            files.add(relative)
    print("\n".join(sorted(files)))
    return 0


def self_test() -> int:
    root = Path(tempfile.mkdtemp())
    failures: list[str] = []

    def expect(name: str, ok: bool) -> None:
        if not ok:
            failures.append(name)

    tidy = (
        f"{root}/libs/core/src/a.cpp:10:5: warning: do not use goto [cppcoreguidelines-avoid-goto]\n"
        f"{root}/libs/core/src/a.cpp:10:5: warning: do not use goto [cppcoreguidelines-avoid-goto]\n"
        "/usr/include/c++/14/vector:5:1: warning: system [bugprone-x]\n"
        f"{root}/build/x/moc_game.cpp:3:1: warning: generated [bugprone-y]\n"
        f"{root}/apps/game/src/b.cpp:7:1: warning: x [clang-diagnostic-unused-variable]\n"
        "libs/core/src/c.cpp:3:1: warning: relative [bugprone-z]\n"
    )
    found = parse("clang-tidy", tidy, root)
    expect("dedupe, system, generated and compiler warnings dropped", [f["check"] for f in found] == ["cppcoreguidelines-avoid-goto", "bugprone-z"])
    msvc = "12>C:\\x\\src\\a.cpp(40,7): warning C6001: Using uninitialized memory 'p'. [C:\\x\\a.vcxproj]\nlibs/core/b.cpp(3): warning C26495: m\n"
    found_msvc = parse("msvc-analyze", msvc, Path("C:/x"))
    expect("msvc style", [(f["check"], f["line"]) for f in found_msvc] == [("C6001", 40), ("C26495", 3)])
    counts = Counter({"a x.cpp": 2, "b y.cpp": 1})
    new, fixed = compare(counts, {"a x.cpp": 1, "c z.cpp": 2})
    expect("compare", new == [("a x.cpp", 1, 2), ("b y.cpp", 0, 1)] and fixed == [("c z.cpp", 2, 0)])
    sarif = to_sarif("t", found)
    expect("sarif", sarif["runs"][0]["results"][0]["locations"][0]["physicalLocation"]["region"]["startLine"] == 10)
    if failures:
        print("static_analysis self-test failed: " + "; ".join(failures))
        return 1
    print("static_analysis self-test passed")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--self-test", action="store_true")
    sub = parser.add_subparsers(dest="command")
    for name in ("check", "lower", "accept"):
        p = sub.add_parser(name)
        p.add_argument("--tool", required=True, choices=TOOLS)
        p.add_argument("--log", required=True, nargs="+", help="console output of the tool")
        p.add_argument("--root", default=".", help="the repository root (default: the current directory)")
        p.add_argument("--sarif", help="write the findings as SARIF to this file")
        p.add_argument("--summary", help="append the Markdown summary to this file (GITHUB_STEP_SUMMARY)")
        p.add_argument("--build-status", type=int, help="exit status of the build the log came from")
    files = sub.add_parser("files")
    files.add_argument("--compile-commands", required=True)
    files.add_argument("--root", default=".")
    args = parser.parse_args()
    if args.self_test:
        return self_test()
    if not args.command:
        parser.print_usage(sys.stderr)
        return 2
    if args.command == "files":
        return list_files(args)
    return run(args)


if __name__ == "__main__":
    sys.exit(main())
