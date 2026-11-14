// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts

// The four answer tiles: a 2 by 2 grid on a phone or a tablet, one row on a desktop window. `values` and `looks` hold one
// entry per tile, in the order of the core's tiles. A tap on a tile sends its position (0 to 3), which is what the core
// takes as an answer.
GridLayout {
    id: root

    required property PlayMetrics metrics
    property var values: []
    property var looks: []
    property bool interactive: true

    signal answered(int position)

    columns: metrics.tileColumns
    rowSpacing: metrics.tileGap
    columnSpacing: metrics.tileGap

    Repeater {
        model: 4

        delegate: AnswerTile {
            id: tile

            required property int index

            objectName: "tile" + index
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            // A tile may shrink from its height to `tileMinHeight` (a layout only shrinks what fills) but never grow.
            Layout.fillHeight: true
            Layout.preferredHeight: root.metrics.tileHeight
            Layout.minimumHeight: root.metrics.tileMinHeight
            Layout.maximumHeight: root.metrics.tileHeight
            value: root.values.length > index ? String(root.values[index]) : ""
            look: root.looks.length > index ? root.looks[index] : "normal"
            interactive: root.interactive
            fontSize: root.metrics.tileFont
            borderWidth: root.metrics.tileBorder
            radius: root.metrics.tileRadius
            shadowOffset: root.metrics.tileShadow
            badgeSize: root.metrics.tileHeight > 120 ? 40 : 30
            onTapped: root.answered(index)
        }
    }
}
