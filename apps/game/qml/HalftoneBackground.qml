// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick

// The yellow page with its grid of halftone dots, behind every screen.
//
// The boards' dots are hard-edged: a pixel is either inside the dot or not, so each dot is painted as whole-pixel runs
// instead of an anti-aliased arc (which comes out two or three pixels wide and soft). The first dot's centre sits half a
// grid step in from the top-left corner, as the boards' repeating background does.
Rectangle {
    id: root

    // The distance between dots and their radius, in pixels. Larger windows use a coarser grid.
    property real gridSize: 12
    property real dotRadius: 1.6

    color: Theme.background

    Canvas {
        id: dots

        anchors.fill: parent
        renderTarget: Canvas.Image
        renderStrategy: Canvas.Cooperative

        onPaint: {
            const ctx = getContext("2d");
            ctx.reset();
            ctx.fillStyle = Theme.halftone;
            const step = root.gridSize;
            const radius = root.dotRadius;
            // The pixel rows a dot covers, as offsets from its centre, with the half-width of each run.
            const runs = [];
            for (let row = Math.floor(-radius); row < Math.ceil(radius); row++) {
                const dy = row + 0.5;
                if (Math.abs(dy) > radius) {
                    continue;
                }
                const half = Math.sqrt(radius * radius - dy * dy);
                const first = Math.ceil(-half - 0.5);
                const last = Math.floor(half - 0.5);
                runs.push({
                    "row": row,
                    "first": first,
                    "count": last - first + 1
                });
            }
            ctx.beginPath();
            for (let y = Math.round(step / 2); y < height + step; y += step) {
                for (let x = Math.round(step / 2); x < width + step; x += step) {
                    for (const run of runs) {
                        ctx.rect(x + run.first, y + run.row, run.count, 1);
                    }
                }
            }
            ctx.fill();
        }
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
    }

    onGridSizeChanged: dots.requestPaint()
    onDotRadiusChanged: dots.requestPaint()
}
