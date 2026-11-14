#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Say whether a built file carries Tracy.

    python3 tools/ci/check_binary_tracy.py --absent FILE...
    python3 tools/ci/check_binary_tracy.py --present FILE...

A build without profiling has no Tracy code in it (CLAUDE.md: no network, nothing that listens; docs/profiling.md). The
tests of the game run this over the program and the core library of every build: --absent where the build has no Tracy,
and --present where it has, so that the check itself cannot go blind. Tracy leaves its name in a binary in every form of
the build: as the import of TracyClient.dll, or as the symbols of a static library (tracy::...).

Exit status 0 when every file is as asked, 1 when one is not, 2 on a usage error.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

# What Tracy leaves in a binary: the import of its library, its namespace as printed or mangled (MSVC and the Itanium
# ABI), or the path of one of its headers. A directory merely named for Tracy is not enough: it is no sign of the code.
TRACY = re.compile(rb"TracyClient|tracy::|@tracy@@|N5tracy|tracy(?:/|\\)Tracy")


def mentions_tracy(path: Path) -> bool:
    return TRACY.search(path.read_bytes()) is not None


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--absent", action="store_true", help="every file must have no mention of Tracy")
    mode.add_argument("--present", action="store_true", help="every file must mention Tracy")
    parser.add_argument("files", nargs="+", type=Path)
    args = parser.parse_args(argv)

    status = 0
    for path in args.files:
        if not path.is_file():
            print(f"{path}: no such file", file=sys.stderr)
            return 2
        found = mentions_tracy(path)
        if found and args.absent:
            print(f"{path}: carries Tracy, and this build is not meant to", file=sys.stderr)
            status = 1
        elif not found and args.present:
            print(f"{path}: has no mention of Tracy, and this build is meant to carry it", file=sys.stderr)
            status = 1
        else:
            print(f"{path}: {'carries' if found else 'has no'} Tracy, as expected")
    return status


if __name__ == "__main__":
    sys.exit(main())
