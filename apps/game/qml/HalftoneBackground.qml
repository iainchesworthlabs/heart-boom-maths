// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick

// The yellow page with its grid of halftone dots, behind every screen. The dots are a small picture of one dot, repeated
// (tools/assets/make_halftone_tiles.py). They used to be painted by a Canvas, which fills in after the screen has appeared
// and showed as a flash of black on a phone; a picture is there from the first frame.
Rectangle {
    id: root

    // The distance between dots and their radius, in pixels. Larger windows use a coarser grid. The pictures exist for the
    // three grids the play screen uses (12, 14 and 16); any other grid uses the one that is nearest.
    property real gridSize: 12
    property real dotRadius: 1.6
    // Where the grid starts, so a page behind another one can keep its dots in step with the page in front.
    property real originX: 0
    property real originY: 0

    readonly property int tileGrid: gridSize < 13 ? 12 : (gridSize < 15 ? 14 : 16)

    color: Theme.background
    clip: true

    Image {
        objectName: "halftoneDots"
        // One tile beyond each edge, shifted by the origin's remainder, so the dots run to every edge whatever the origin.
        readonly property real step: root.tileGrid
        x: ((root.originX % step) + step) % step - step
        y: ((root.originY % step) + step) % step - step
        width: root.width + 2 * step
        height: root.height + 2 * step
        fillMode: Image.Tile
        smooth: false
        asynchronous: false
        cache: true
        source: "qrc:/heart_boom_maths/images/halftone/dots-" + root.tileGrid + ".png"
    }
}
