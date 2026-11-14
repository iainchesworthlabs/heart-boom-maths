#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Run the pull request's static checks locally.

    python3 tools/ci/precheck.py [--base develop] [--root .]

Finds a checkout of icl-common-infra in ICL_COMMON_INFRA, then ../icl-common-infra, and runs
its scripts/precheck.py with the same arguments. These are the checks the Policy job runs. It then runs this
project's own checks (tools/ci/check_*.py), which the "Project rules" job of ci.yml runs too.
"""

from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent


def find_infra() -> Path | None:
    candidates = []
    if os.environ.get("ICL_COMMON_INFRA"):
        candidates.append(Path(os.environ["ICL_COMMON_INFRA"]))
    candidates += [ROOT.parent / "icl-common-infra", ROOT / ".icl-infra"]
    for path in candidates:
        if (path / "scripts" / "precheck.py").is_file():
            return path
    return None


# The project's own rules (CLAUDE.md), run by the "Project rules" job of ci.yml as well. Each script is run on the
# repository and then against its own self-test, so a checker that stopped finding anything cannot pass silently.
PROJECT_CHECKS = [
    ("core includes", "check_core_includes.py"),
    ("single link-open call", "check_link_open.py"),
    ("web page is closed", "check_web_site.py"),
    ("no Tracy in mobile, web and instrumented presets", "check_presets_tracy.py"),
]


def run_project_checks(root: Path) -> int:
    status = 0
    for name, script in PROJECT_CHECKS:
        script_path = str(HERE / script)
        problems = []
        for extra in ([], ["--self-test"]):
            res = subprocess.run([sys.executable, script_path, "--root", str(root), *extra],
                                 capture_output=True, text=True, check=False)
            if res.returncode != 0:
                problems += (res.stdout + res.stderr).strip().splitlines()[-10:]
        if problems:
            status = 1
            print(f"FAIL  {name}")
            for line in problems:
                print(f"      {line}")
        else:
            print(f"PASS  {name}")
    return status


def main() -> int:
    infra = find_infra()
    if infra is None:
        print("icl-common-infra was not found. Clone it next to this repository or set "
              "ICL_COMMON_INFRA to its path.", file=sys.stderr)
        return 2
    args = sys.argv[1:]
    if "--root" not in args:
        args += ["--root", str(ROOT)]
    shared = subprocess.call([sys.executable, str(infra / "scripts" / "precheck.py"), *args])
    root = Path(args[args.index("--root") + 1])
    project = run_project_checks(root)
    return shared or project


if __name__ == "__main__":
    sys.exit(main())
