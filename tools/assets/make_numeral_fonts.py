#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Derive the two numeral fonts: Bangers and Lilita One with a crossed 7, so no child reads a 7 as a 1.

    python3 tools/assets/make_numeral_fonts.py                 write HBMDisplay-Regular.ttf and HBMBody-Regular.ttf
    python3 tools/assets/make_numeral_fonts.py --foot          also give the 1 a base foot (a stronger change)
    python3 tools/assets/make_numeral_fonts.py --out DIR       write somewhere else

Needs `pip install fonttools skia-pathops`. It is a design tool, not part of the build: the two derived fonts are committed
under apps/game/fonts/, and CI does not run this.

Only the glyph `7` changes (and `1` with --foot). A bar is added across the stem of the 7 at about a third of its height and
merged into the outline, so the digit is one clean shape with no overlapping contours. The hinting of those glyphs is
dropped (the old instructions no longer fit them); everything else is byte for byte what the original font holds, except the
names. Both licences are the SIL Open Font License 1.1, which asks a modified font to carry the copyright and licence and
not to use a Reserved Font Name: Lilita One reserves "Lilita One", so both derived families get new names (HBM Display, HBM
Body), and the signature table of the original is dropped because it no longer matches.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import pathops
from fontTools.pens.recordingPen import RecordingPen
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.ttLib import TTFont

FONTS = Path(__file__).resolve().parents[2] / "apps" / "game" / "fonts"

# Units are 1000 to the em. `left` and `right` are the two edges of the stem of the 7 (a line each, as two points) and `y` is
# the height of the bar's centre line; the bar reaches `ext_l` and `ext_r` past the stem and tilts by `tilt`. `shift` moves
# the whole 7 right and `advance_add` widens it so the bar does not touch the digit before it. `radius` rounds the bar's
# corners to match Lilita One's soft ends. `one` is the foot of the 1: its two ends, its thickness and the new width.
SPECS = {
    "display": {
        "src": "Bangers-Regular.ttf",
        "family": "HBM Display",
        "ps": "HBMDisplay",
        "seven": {"left": ((29, 9), (228, 556)), "right": ((208, 25), (462, 740)), "y": 300, "thick": 112,
                  "ext_l": 95, "ext_r": 140, "tilt": 0.10, "radius": 0, "shift": 45, "advance_add": 60},
        "one": {"x0": 40, "x1": 335, "y_left": -4, "tilt": 0.12, "thick": 100, "radius": 0, "advance": 345},
    },
    "body": {
        "src": "LilitaOne-Regular.ttf",
        "family": "HBM Body",
        "ps": "HBMBody",
        "seven": {"left": ((90, 13), (257, 503)), "right": ((313, 13), (485, 608)), "y": 262, "thick": 112,
                  "ext_l": 85, "ext_r": 110, "tilt": 0.0, "radius": 32, "shift": 40, "advance_add": 55},
        "one": {"x0": 45, "x1": 400, "y_left": -3, "tilt": 0.0, "thick": 100, "radius": 30, "advance": 425},
    },
}


def quad_pen(pen, corners, radius):
    """Draw a closed four-sided shape; with a radius its corners are rounded with quadratic curves."""
    if radius <= 0:
        pen.moveTo(corners[0])
        for corner in corners[1:]:
            pen.lineTo(corner)
        pen.closePath()
        return

    def toward(a, b, distance):
        dx, dy = b[0] - a[0], b[1] - a[1]
        length = (dx * dx + dy * dy) ** 0.5
        return (a[0] + dx / length * distance, a[1] + dy / length * distance)

    for i, corner in enumerate(corners):
        before = toward(corner, corners[i - 1], radius)
        after = toward(corner, corners[(i + 1) % len(corners)], radius)
        if i == 0:
            pen.moveTo(before)
        else:
            pen.lineTo(before)
        pen.qCurveTo(corner, after)
    pen.closePath()


def union(glyph_set, name, shapes):
    """The glyph's outline merged with extra shapes. Each shape is wound on its own first: drawn the other way round it
    would cancel the outline where the two overlap."""
    recording = RecordingPen()
    glyph_set[name].draw(recording)
    path = pathops.Path()
    recording.replay(path.getPen())
    path.simplify(fix_winding=True)
    for draw in shapes:
        extra = pathops.Path()
        draw(extra.getPen())
        extra.simplify(fix_winding=True)
        path = pathops.op(path, extra, pathops.PathOp.UNION)
    path.simplify(fix_winding=True)
    pen = TTGlyphPen(None)
    path.draw(pen)
    return pen.glyph()


def bar(x0, x1, y0, y1, thickness):
    half = thickness / 2.0
    return [(x0, y0 - half), (x1, y1 - half), (x1, y1 + half), (x0, y0 + half)]


def x_on_edge(edge, y):
    (xa, ya), (xb, yb) = edge
    return xa + (y - ya) * (xb - xa) / (yb - ya)


def build(source: Path, out: Path, spec: dict, foot: bool) -> None:
    font = TTFont(source, recalcTimestamp=False)
    glyphs, cmap, glyf, hmtx = font.getGlyphSet(), font.getBestCmap(), font["glyf"], font["hmtx"]
    seven, one = cmap[ord("7")], cmap[ord("1")]

    s = spec["seven"]
    left, right = x_on_edge(s["left"], s["y"]), x_on_edge(s["right"], s["y"])
    x0, x1 = left - s["ext_l"], right + s["ext_r"]
    centre = (left + right) / 2.0
    corners = bar(x0, x1, s["y"] + s["tilt"] * (x0 - centre), s["y"] + s["tilt"] * (x1 - centre), s["thick"])
    glyph = union(glyphs, seven, [lambda pen: quad_pen(pen, corners, s["radius"])])
    glyph.coordinates.translate((s["shift"], 0))
    glyf[seven] = glyph
    hmtx[seven] = (hmtx[seven][0] + s["advance_add"], 0)
    changed = [seven]

    if foot:
        o = spec["one"]
        y_start = o["y_left"] + o["thick"] / 2.0
        y_end = y_start + o["tilt"] * (o["x1"] - o["x0"])
        corners = bar(o["x0"], o["x1"], y_start, y_end, o["thick"])
        glyf[one] = union(glyphs, one, [lambda pen: quad_pen(pen, corners, o["radius"])])
        hmtx[one] = (o["advance"], 0)
        changed.append(one)

    for name in changed:
        glyf[name].recalcBounds(glyf)
        hmtx[name] = (hmtx[name][0], glyf[name].xMin)

    names = font["name"]
    for record in list(names.names):
        if record.nameID in (3, 16, 17, 21, 22, 25):
            names.removeNames(nameID=record.nameID)
    note = "Modified: the 7 has a crossbar" + (" and the 1 has a foot" if foot else "") + ", so the two are not mistaken."
    for platform, encoding, language in ((3, 1, 0x409), (1, 0, 0)):
        names.setName(spec["family"], 1, platform, encoding, language)
        names.setName("Regular", 2, platform, encoding, language)
        names.setName(spec["family"] + " Regular; modified", 3, platform, encoding, language)
        names.setName(spec["family"] + " Regular", 4, platform, encoding, language)
        names.setName(spec["ps"] + "-Regular", 6, platform, encoding, language)
        names.setName(note, 10, platform, encoding, language)
    for table in ("DSIG", "FFTM"):
        if table in font:
            del font[table]
    out.parent.mkdir(parents=True, exist_ok=True)
    font.save(out)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--foot", action="store_true", help="also give the 1 a base foot")
    parser.add_argument("--src", type=Path, default=FONTS, help="folder of the original fonts")
    parser.add_argument("--out", type=Path, default=FONTS, help="folder to write to")
    args = parser.parse_args()
    for spec in SPECS.values():
        target = args.out / f"{spec['ps']}-Regular.ttf"
        build(args.src / spec["src"], target, spec, args.foot)
        print(target)
    return 0


if __name__ == "__main__":
    sys.exit(main())
