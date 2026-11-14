#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Draw the placeholder app icon of the iOS build: a flat red heart with a fuse on a flat yellow square.

    python3 tools/assets/make_placeholder_icon.py            write apps/game/ios/Assets.xcassets/AppIcon.appiconset/AppIcon-1024.png
    python3 tools/assets/make_placeholder_icon.py --check    fail when the file differs from the drawing

The icon is a PLACEHOLDER. The real icon has not been designed, and a designer's artwork replaces this file (docs/ios.md).
It is drawn here from a formula, with flat colours and no outside art, so it is entirely this project's own work and
carries its licence. The picture is a full square with no transparency, as the App Store requires: iOS rounds the corners.

The picture is the same on every machine: the script uses only the standard library, only + - * / on floating point
numbers (which IEEE 754 defines exactly) and four samples to a pixel. --check compares the decoded pixels and not the bytes
of the file, because the bytes of a compressed PNG depend on the zlib that wrote it.
"""

from __future__ import annotations

import argparse
import struct
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
DEFAULT_OUTPUT = ROOT / "apps" / "game" / "ios" / "Assets.xcassets" / "AppIcon.appiconset" / "AppIcon-1024.png"

SIZE = 1024
BACKGROUND = (255, 201, 60)
HEART = (229, 56, 59)
FUSE = (60, 36, 28)
SPARK = (255, 255, 255)

# The heart is the curve (x^2 + y^2 - 1)^3 - x^2 y^3 <= 0, which spans about -1.14 to 1.14 across and -1.0 to 1.25 up.
# SCALE maps one unit of the curve to pixels; the centre is moved down a little to leave room for the fuse above.
HEART_SCALE = 330.0
HEART_CENTRE = (512.0, 600.0)

# The fuse is a thick line from the notch of the heart up to the right, and the spark is a disc at its end.
FUSE_START = (512.0, 300.0)
FUSE_END = (596.0, 128.0)
FUSE_HALF_WIDTH = 18.0
SPARK_CENTRE = (606.0, 104.0)
SPARK_RADIUS = 38.0

SAMPLES = (0.25, 0.75)


def in_heart(px: float, py: float) -> bool:
    x = (px - HEART_CENTRE[0]) / HEART_SCALE
    y = (HEART_CENTRE[1] - py) / HEART_SCALE
    a = x * x + y * y - 1.0
    return a * a * a - x * x * y * y * y <= 0.0


def in_fuse(px: float, py: float) -> bool:
    dx = FUSE_END[0] - FUSE_START[0]
    dy = FUSE_END[1] - FUSE_START[1]
    length2 = dx * dx + dy * dy
    t = ((px - FUSE_START[0]) * dx + (py - FUSE_START[1]) * dy) / length2
    t = 0.0 if t < 0.0 else 1.0 if t > 1.0 else t
    qx = FUSE_START[0] + t * dx - px
    qy = FUSE_START[1] + t * dy - py
    return qx * qx + qy * qy <= FUSE_HALF_WIDTH * FUSE_HALF_WIDTH


def in_spark(px: float, py: float) -> bool:
    dx = px - SPARK_CENTRE[0]
    dy = py - SPARK_CENTRE[1]
    return dx * dx + dy * dy <= SPARK_RADIUS * SPARK_RADIUS


def sample_colour(px: float, py: float) -> tuple[int, int, int]:
    if in_spark(px, py):
        return SPARK
    if in_fuse(px, py):
        return FUSE
    if in_heart(px, py):
        return HEART
    return BACKGROUND


def render() -> bytes:
    """The picture as RGB rows, each starting with the PNG filter byte 0 (none)."""
    rows = bytearray()
    for row in range(SIZE):
        rows.append(0)
        for column in range(SIZE):
            red = green = blue = 0
            for sy in SAMPLES:
                for sx in SAMPLES:
                    colour = sample_colour(column + sx, row + sy)
                    red += colour[0]
                    green += colour[1]
                    blue += colour[2]
            rows.extend((red // 4, green // 4, blue // 4))
    return bytes(rows)


def chunk(kind: bytes, data: bytes) -> bytes:
    body = kind + data
    return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)


def encode_png(rows: bytes) -> bytes:
    header = struct.pack(">IIBBBBB", SIZE, SIZE, 8, 2, 0, 0, 0)  # 8-bit RGB, no alpha channel, not interlaced
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b"")


def decode_rows(png: bytes) -> bytes:
    """The rows of a PNG this script wrote, or an empty bytes object for anything else."""
    if png[:8] != b"\x89PNG\r\n\x1a\n":
        return b""
    position = 8
    idat = bytearray()
    while position + 8 <= len(png):
        (length,) = struct.unpack(">I", png[position : position + 4])
        kind = png[position + 4 : position + 8]
        data = png[position + 8 : position + 8 + length]
        if kind == b"IHDR" and data != struct.pack(">IIBBBBB", SIZE, SIZE, 8, 2, 0, 0, 0):
            return b""
        if kind == b"IDAT":
            idat.extend(data)
        position += 12 + length
    try:
        return zlib.decompress(bytes(idat))
    except zlib.error:
        return b""


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true", help="fail when the file differs from the drawing")
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT, help="the PNG to write or check")
    args = parser.parse_args()

    rows = render()
    if args.check:
        if not args.output.is_file() or decode_rows(args.output.read_bytes()) != rows:
            print(f"{args.output} differs from tools/assets/make_placeholder_icon.py: run the script and commit the file")
            return 1
        print(f"{args.output} matches the drawing")
        return 0
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(encode_png(rows))
    print(f"wrote {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
