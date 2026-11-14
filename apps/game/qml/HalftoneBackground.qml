// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick

// The yellow page with its grid of halftone dots, behind every screen.
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
            ctx.beginPath();
            const step = root.gridSize;
            for (let y = step / 2; y < height + step; y += step) {
                for (let x = step / 2; x < width + step; x += step) {
                    ctx.moveTo(x + root.dotRadius, y);
                    ctx.arc(x, y, root.dotRadius, 0, 2 * Math.PI);
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
