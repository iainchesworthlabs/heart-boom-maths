// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick

// A card with a thick ink outline and the hard offset shadow (no blur). Children go inside the outline: their parent is
// the area the border leaves free, so a margin on a child is measured from the inside of the border, as CSS padding is on
// the boards.
Item {
    id: root

    property color fillColor: Theme.white
    property color borderColor: Theme.ink
    property real borderWidth: Theme.outlineLarge
    property real radius: Theme.radiusLarge
    // How far the shadow sits to the right and below. 0 draws none.
    property real shadowOffset: Theme.shadowLarge
    property bool dashed: false

    default property alias content: inner.data
    readonly property alias body: body
    // The free area inside the border.
    readonly property alias inner: inner

    Rectangle {
        visible: root.shadowOffset > 0
        x: root.shadowOffset
        y: root.shadowOffset
        width: root.width
        height: root.height
        radius: root.radius
        color: Theme.ink
    }

    Rectangle {
        id: body

        anchors.fill: parent
        radius: root.radius
        color: root.fillColor
        border.width: root.dashed ? 0 : root.borderWidth
        border.color: root.borderColor
    }

    Item {
        id: inner

        anchors.fill: parent
        anchors.margins: root.borderWidth
    }

    DashedBox {
        anchors.fill: parent
        visible: root.dashed
        lineColor: root.borderColor
        lineWidth: root.borderWidth
        radius: root.radius
    }
}
