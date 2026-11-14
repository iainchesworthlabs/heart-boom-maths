#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Run the game for a few seconds and say whether anything listens on Tracy's port.

    python3 tools/ci/check_listener.py --silent    PROGRAM [ARGS...]
    python3 tools/ci/check_listener.py --listening PROGRAM [ARGS...]

The game may carry Tracy's client, dormant (docs/profiling.md). The client opens a listening socket only when the game is
asked to profile. This starts PROGRAM (with --smoke-test and --smoke-hold-ms, so that it stays up after its first frame),
connects to the profiler's port on the loopback interface for the whole of that time, and checks, for each way of asking:

  --silent     nothing is ever listening, and the program never says it started the profiler
  --listening  the loopback port accepts a connection, the machine's own network address does not (the listener is bound
               to the loopback interface only), and the program says it started the profiler exactly once

The port is TRACY_PORT, set here, so that a profiler open on the developer's machine does not disturb the check.

Exit status 0 when as asked, 1 when not, 2 on a usage error.
"""

from __future__ import annotations

import argparse
import os
import socket
import subprocess
import sys
import time

STARTED = "Tracy profiler started"
HOLD_MS = 6000
POLL_SECONDS = 0.1


def can_connect(host: str, port: int) -> bool:
    try:
        with socket.create_connection((host, port), timeout=0.2):
            return True
    except OSError:
        return False


def own_address() -> str | None:
    """An address of this machine that is not the loopback one, or None when there is none to try."""
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
            probe.connect(("192.0.2.1", 9))  # a documentation address: nothing is sent
            address = probe.getsockname()[0]
    except OSError:
        return None
    return None if address.startswith("127.") else address


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--silent", action="store_true")
    mode.add_argument("--listening", action="store_true")
    parser.add_argument("--port", type=int, default=18086)
    parser.add_argument("program", nargs=argparse.REMAINDER)
    args = parser.parse_args(argv)
    if not args.program:
        parser.error("no program given")

    env = dict(os.environ, TRACY_PORT=str(args.port))
    command = [*args.program, "--smoke-test", "--smoke-hold-ms", str(HOLD_MS)]
    process = subprocess.Popen(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)

    connected_loopback = False
    connected_network = False
    network = own_address()
    deadline = time.monotonic() + HOLD_MS / 1000.0
    while process.poll() is None and time.monotonic() < deadline:
        connected_loopback = connected_loopback or can_connect("127.0.0.1", args.port)
        if network is not None:
            connected_network = connected_network or can_connect(network, args.port)
        time.sleep(POLL_SECONDS)
    try:
        output, _ = process.communicate(timeout=60)
    except subprocess.TimeoutExpired:
        process.kill()
        output, _ = process.communicate()
    started = output.count(STARTED)

    problems: list[str] = []
    if args.silent:
        if connected_loopback or connected_network:
            problems.append(f"something listens on port {args.port} although the game was not asked to profile")
        if started:
            problems.append(f'the program said "{STARTED}" although it was not asked to profile')
    else:
        if not connected_loopback:
            problems.append(f"nothing accepted a connection on 127.0.0.1:{args.port}")
        if connected_network:
            problems.append(f"the listener answers on {network}:{args.port}: it must be bound to the loopback interface")
        if started != 1:
            problems.append(f'the program said "{STARTED}" {started} times, not once')
    if network is None and args.listening:
        print("note: no network address of this machine was found to try, so the loopback-only check did not run")
    for problem in problems:
        print(f"FAIL {problem}", file=sys.stderr)
    if problems:
        print(output, file=sys.stderr)
        return 1
    print(f"ok: {'nothing listening' if args.silent else 'listening on the loopback interface only, started once'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
