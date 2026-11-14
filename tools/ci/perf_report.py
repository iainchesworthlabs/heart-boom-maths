#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Summarise a run of the performance tests, and retune their budgets from a local run.

    python3 tools/ci/perf_report.py [--results DIR] [--markdown FILE]
    python3 tools/ci/perf_report.py --update-budgets [--results DIR] [--headroom 4] [--count-headroom 2] [--machine TEXT]
    python3 tools/ci/perf_report.py --self-test

Each performance test writes one `<name>.json` into the results directory (HBM_PERF_RESULTS_DIR, which CTest sets to
`<build>/perf-results`): its median, best, spread and the budget it was held to. This script reads them all and

  - prints a table and writes `summary.json` and `summary.csv` beside them, for tracking from one run to the next;
  - with --markdown, also writes the table as Markdown (CI appends it to the job summary);
  - exits 1 when any result failed its budget.

--update-budgets rewrites tools/perf-budgets.json from the results instead: the budget of every budgeted test becomes its
median times the headroom (4 for a time, 2 for a count), rounded up to two significant figures and never below a small
floor (100 ns, 50 us, 4 MiB), and the entry
records the machine it was set on. It is for a person at a desk after a run with HBM_PERF_NO_BUDGET=1 (so a missing
budget does not fail the run), and it refuses to run in CI.

Exit status 0 when every test passed (or the budgets were written), 1 on a failed budget, 2 on a usage error.
"""

from __future__ import annotations

import argparse
import csv
import json
import math
import os
import platform
import sys
import tempfile
from pathlib import Path

BUDGET_FILE = Path("tools/perf-budgets.json")
DEFAULT_RESULTS = [Path("build/windows-msvc-release-perf/perf-results"), Path("build/linux-release-perf/perf-results")]
COUNT_UNITS = {"milli-allocs/op", "bytes", "objects"}
# A budget never goes below this, whatever the median said: under 100 ns a time is mostly the clock, and a process grows
# and shrinks by a few pages for reasons no code of ours controls.
MIN_BUDGET = {"ns": 100, "us": 50, "bytes": 4 * 1024 * 1024}
SUMMARY_NAMES = {"summary.json"}


def find_results_dir(given: str | None) -> Path | None:
    if given:
        return Path(given)
    for candidate in DEFAULT_RESULTS:
        if candidate.is_dir():
            return candidate
    return None


def load_results(directory: Path) -> list[dict]:
    results = []
    for path in sorted(directory.glob("*.json")):
        if path.name in SUMMARY_NAMES:
            continue
        results.append(json.loads(path.read_text(encoding="utf-8")))
    return sorted(results, key=lambda r: r["name"])


def human(value: float, unit: str) -> str:
    if unit == "ns" and value >= 1_000_000:
        return f"{value / 1_000_000:.2f} ms"
    if unit == "ns" and value >= 1_000:
        return f"{value / 1_000:.2f} us"
    if unit == "us" and value >= 1_000:
        return f"{value / 1_000:.2f} ms"
    return f"{value:.4g} {unit}"


def render_table(results: list[dict], markdown: bool) -> str:
    rows = []
    for r in results:
        budget = r.get("budget")
        rows.append((r["name"], r["metric"], human(r["median"], r["unit"]), human(r["best"], r["unit"]),
                     "-" if budget is None else human(budget, r["unit"]),
                     "pass" if r.get("pass", True) else "FAIL"))
    header = ("test", "metric", "median", "best", "budget", "result")
    if markdown:
        lines = ["| " + " | ".join(header) + " |", "|" + "---|" * len(header)]
        lines += ["| " + " | ".join(row) + " |" for row in rows]
        return "\n".join(lines) + "\n"
    widths = [max(len(header[i]), *(len(row[i]) for row in rows)) if rows else len(header[i]) for i in range(len(header))]
    lines = ["  ".join(header[i].ljust(widths[i]) for i in range(len(header)))]
    lines += ["  ".join(row[i].ljust(widths[i]) for i in range(len(header))) for row in rows]
    return "\n".join(lines) + "\n"


def write_summaries(results: list[dict], directory: Path) -> None:
    (directory / "summary.json").write_text(json.dumps({"results": results}, indent=1) + "\n", encoding="utf-8")
    keys = ["name", "metric", "unit", "median", "best", "mean", "p95", "p99", "max", "repetitions", "budget", "pass"]
    with open(directory / "summary.csv", "w", newline="", encoding="utf-8") as out:
        writer = csv.writer(out)
        writer.writerow(keys)
        for r in results:
            writer.writerow([r.get(k, "") if r.get(k) is not None else "" for k in keys])


def round_up_two_figures(value: float) -> int:
    if value <= 0:
        return 0
    if value < 10:
        return math.ceil(value)
    step = 10 ** (math.floor(math.log10(value)) - 1)
    return int(math.ceil(value / step) * step)


def describe_machine() -> str:
    cpu = platform.processor() or platform.machine()
    return f"{cpu}, {platform.system()} {platform.release()}, {os.cpu_count()} logical cores"


def make_budgets(results: list[dict], existing: dict, headroom: float, count_headroom: float, machine: str) -> dict:
    """The budget file's content with every budgeted result's entry set from its median. Entries without a result stay."""
    entries = {e["name"]: e for e in existing.get("budgets", [])}
    for r in results:
        if not r.get("budgeted", False):
            continue
        factor = count_headroom if r["unit"] in COUNT_UNITS else headroom
        entries[r["name"]] = {
            "name": r["name"],
            "metric": r["metric"],
            "unit": r["unit"],
            "budget": max(round_up_two_figures(r["median"] * factor), MIN_BUDGET.get(r["unit"], 0)),
            "median_when_set": int(round(r["median"])),
            "machine": machine,
        }
    return {
        "version": 1,
        "note": existing.get("note", ""),
        # Whole numbers: the game's JSON reader, which reads this file, has no fractions.
        "headroom": {"time": int(math.ceil(headroom)), "count": int(math.ceil(count_headroom))},
        "budgets": [entries[name] for name in sorted(entries)],
    }


def format_budgets(document: dict) -> str:
    """One entry to a line, so a changed budget reviews as a one-line diff."""
    head = [
        "{",
        '  "version": 1,',
        f'  "note": {json.dumps(document["note"])},',
        f'  "headroom": {json.dumps(document["headroom"])},',
        '  "budgets": [',
    ]
    body = [("    " + json.dumps(e) + ",") for e in document["budgets"]]
    if body:
        body[-1] = body[-1].rstrip(",")
    return "\n".join(head + body + ["  ]", "}"]) + "\n"


def update_budgets(results_dir: Path, headroom: float, count_headroom: float, machine: str) -> int:
    if os.environ.get("CI") or os.environ.get("GITHUB_ACTIONS"):
        print("perf_report: --update-budgets never runs in CI; run it at a desk after a local run.", file=sys.stderr)
        return 2
    results = load_results(results_dir)
    if not results:
        print(f"perf_report: no results in {results_dir}; run the perf tests first.", file=sys.stderr)
        return 2
    existing = json.loads(BUDGET_FILE.read_text(encoding="utf-8")) if BUDGET_FILE.exists() else {}
    document = make_budgets(results, existing, headroom, count_headroom, machine)
    BUDGET_FILE.write_text(format_budgets(document), encoding="utf-8", newline="\n")
    print(f"perf_report: wrote {len(document['budgets'])} budgets to {BUDGET_FILE} (median x {headroom}, counts x "
          f"{count_headroom}, set on: {machine})")
    return 0


def self_test() -> int:
    ok = True

    def expect(name: str, got, want) -> None:
        nonlocal ok
        good = got == want
        ok &= good
        print(f"{'ok  ' if good else 'FAIL'} {name}" + ("" if good else f"  got {got!r}, want {want!r}"))

    expect("round 0", round_up_two_figures(0), 0)
    expect("round small", round_up_two_figures(3.2), 4)
    expect("round 2 figures", round_up_two_figures(123), 130)
    expect("round exact", round_up_two_figures(120), 120)
    expect("round big", round_up_two_figures(4_567_890), 4_600_000)
    results = [
        {"name": "b.time", "metric": "ns_per_op", "unit": "ns", "median": 25.0, "best": 24, "budgeted": True},
        {"name": "a.allocs", "metric": "allocs", "unit": "milli-allocs/op", "median": 0.0, "best": 0, "budgeted": True},
        {"name": "c.info", "metric": "x", "unit": "us", "median": 9.0, "best": 9, "budgeted": False},
    ]
    document = make_budgets(results, {"note": "n", "budgets": [{"name": "z.keep", "metric": "m", "unit": "ns",
                                                                "budget": 5}]}, 4, 2, "box")
    names = [e["name"] for e in document["budgets"]]
    expect("entries sorted, info left out, others kept", names, ["a.allocs", "b.time", "z.keep"])
    expect("time budget has headroom", document["budgets"][1]["budget"], 100)
    expect("count budget of zero stays zero", document["budgets"][0]["budget"], 0)
    expect("a tiny time gets the floor", make_budgets([{"name": "t", "metric": "m", "unit": "ns", "median": 6.0,
                                                          "budgeted": True}], {}, 4, 2, "box")["budgets"][0]["budget"], 100)
    text = format_budgets(document)
    expect("formatted file is JSON", json.loads(text)["budgets"][1]["name"], "b.time")
    expect("one entry per line", sum(1 for line in text.splitlines() if '"name"' in line), 3)
    with tempfile.TemporaryDirectory() as tmp:
        directory = Path(tmp)
        for r in results:
            (directory / f"{r['name']}.json").write_text(json.dumps(r), encoding="utf-8")
        write_summaries(load_results(directory), directory)
        expect("csv written", (directory / "summary.csv").read_text(encoding="utf-8").splitlines()[0].startswith("name,"), True)
        expect("results reload without the summary", len(load_results(directory)), 3)
    return 0 if ok else 1


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--results", help="directory of result files (default: the build directory of a perf preset)")
    parser.add_argument("--markdown", help="also write the table as Markdown to this file")
    parser.add_argument("--update-budgets", action="store_true", help="rewrite tools/perf-budgets.json from the results")
    parser.add_argument("--headroom", type=float, default=4, help="budget = median x this, for a time (default 4)")
    parser.add_argument("--count-headroom", type=float, default=2, help="the same for an allocation count (default 2)")
    parser.add_argument("--machine", default=None, help="the machine the budgets are set on (default: this one)")
    parser.add_argument("--self-test", action="store_true", help="run the script's own tests and exit")
    args = parser.parse_args(argv)
    if args.self_test:
        return self_test()
    directory = find_results_dir(args.results)
    if directory is None or not directory.is_dir():
        print("perf_report: no results directory found; run the perf tests, or pass --results.", file=sys.stderr)
        return 2
    if args.update_budgets:
        return update_budgets(directory, args.headroom, args.count_headroom, args.machine or describe_machine())
    results = load_results(directory)
    if not results:
        print(f"perf_report: no results in {directory}.", file=sys.stderr)
        return 2
    write_summaries(results, directory)
    print(render_table(results, markdown=False), end="")
    if args.markdown:
        Path(args.markdown).write_text(render_table(results, markdown=True), encoding="utf-8", newline="\n")
    failed = [r["name"] for r in results if not r.get("pass", True)]
    if failed:
        print(f"{len(failed)} result(s) over budget: {', '.join(failed)}", file=sys.stderr)
        return 1
    print(f"{len(results)} results, all within budget (summary in {directory}).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
