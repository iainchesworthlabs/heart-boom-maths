#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Draw the tiles of the halftone page: one dot on a transparent square, repeated by the page to make the grid of dots.

    python3 tools/assets/make_halftone_tiles.py            write apps/game/images/halftone/dots-<grid>[@2x|@3x].png
    python3 tools/assets/make_halftone_tiles.py --check    fail when a file differs from the drawing

A tile is `grid` pixels square with one dot of radius `radius` at its centre, in the halftone colour, anti-aliased. The page
uses three grids (the phone, the tablet and the desktop play screens: PlayMetrics.qml), and each comes at 1x, 2x and 3x
(`@2x`, `@3x`) because Qt picks the file that matches the screen. The tile replaces a Canvas that painted the same dots at
run time: a Canvas fills in after the screen appears, and on a phone it showed as a flash of black.

The pictures are the same on every machine: the script uses only the standard library, only + - * / on floating point
numbers and 8 x 8 samples to a pixel. --check compares the decoded pixels, because the bytes of a compressed PNG depend on
the zlib that wrote them.
"""

from __future__ import annotations

import argparse
import struct
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
OUT = ROOT / "apps" / "game" / "images" / "halftone"

# The halftone colour of the design tokens (Theme.halftone), and the grids with the dot radius each one uses.
COLOUR = (0xF5, 0xB8, 0x00)
GRIDS = ((12, 1.6), (14, 2.0), (16, 2.2))
SCALES = ((1, ""), (2, "@2x"), (3, "@3x"))
SAMPLES = 8


def render(grid: int, radius: float, scale: int) -> tuple[int, bytes]:
    """The RGBA rows of one tile at one scale: (size in pixels, bytes with a filter byte before each row)."""
    size = grid * scale
    centre = size / 2.0
    pixel_radius = radius * scale
    rows = bytearray()
    for y in range(size):
        rows.append(0)  # filter type 0: none
        for x in range(size):
            inside = 0
            for sy in range(SAMPLES):
                for sx in range(SAMPLES):
                    px = x + (sx + 0.5) / SAMPLES - centre
                    py = y + (sy + 0.5) / SAMPLES - centre
                    if px * px + py * py <= pixel_radius * pixel_radius:
                        inside += 1
            alpha = (inside * 255 + (SAMPLES * SAMPLES) // 2) // (SAMPLES * SAMPLES)
            rows.extend((COLOUR[0], COLOUR[1], COLOUR[2], alpha))
    return size, bytes(rows)


def chunk(kind: bytes, data: bytes) -> bytes:
    body = kind + data
    return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)


def encode_png(size: int, rows: bytes) -> bytes:
    header = struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0)  # 8 bit, RGBA
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b"")


def decode_rows(png: bytes) -> bytes:
    """The raw filtered rows of a PNG made by encode_png (one IDAT chunk)."""
    position = 8
    data = b""
    while position < len(png):
        length = struct.unpack(">I", png[position : position + 4])[0]
        kind = png[position + 4 : position + 8]
        if kind == b"IDAT":
            data += png[position + 8 : position + 8 + length]
        position += 12 + length
    return zlib.decompress(data)


def files() -> dict[Path, tuple[int, bytes]]:
    made = {}
    for grid, radius in GRIDS:
        for scale, suffix in SCALES:
            made[OUT / f"dots-{grid}{suffix}.png"] = render(grid, radius, scale)
    return made


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--check", action="store_true", help="fail when a file differs from the drawing")
    args = parser.parse_args()

    failed = False
    for path, (size, rows) in files().items():
        if args.check:
            if not path.exists():
                print(f"missing: {path.relative_to(ROOT)}", file=sys.stderr)
                failed = True
            elif decode_rows(path.read_bytes()) != rows:
                print(f"differs from the drawing: {path.relative_to(ROOT)}", file=sys.stderr)
                failed = True
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(encode_png(size, rows))
            print(f"wrote {path.relative_to(ROOT)} ({size} x {size})")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
