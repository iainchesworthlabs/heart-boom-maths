// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts

// The page the result and score screens stand on: the halftone background and one centred column that is 342 wide at
// unit 1, the width of the phone boards. Every size inside the column is a number times `unit`, so a wider window scales
// the same arrangement up, and a window that is short for the content scales it down instead of cutting it off.
//
// A screen puts its content in as children (they are the column's children, so Layout properties work) and says how tall
// that content is at unit 1 in `designHeight`.
Item {
    id: root

    // The content's height at unit 1, from the board.
    property real designHeight: 640
    // The widest the column may be, in pixels: a tablet or a desktop window gets a larger column but not an endless one.
    property real maxWidth: 480
    // Where the column starts on a phone, measured from the top: the boards leave about this much above the first element.
    property real topMax: 64
    // Room at the bottom the column must not use, such as an on-screen keyboard.
    property real reservedBottom: 0
    // True: the column starts `topMax` from the top whenever it fits, instead of being centred in the room that is left.
    property bool pinTop: false
    property real columnSpacing: 20

    readonly property real sideMargin: 24
    readonly property real verticalMargin: 20
    readonly property real availableHeight: Math.max(1, height - reservedBottom)
    readonly property real unit: Math.max(0.5, Math.min(Math.min(width - 2 * sideMargin, maxWidth) / 342, (availableHeight - 2 * verticalMargin) / designHeight))
    readonly property real columnWidth: 342 * unit
    readonly property bool compact: Theme.sizeClassFor(width) === "compact"
    // The height the content really takes.
    readonly property real contentHeight: column.implicitHeight

    default property alias content: column.data

    HalftoneBackground {
        anchors.fill: parent
    }

    ColumnLayout {
        id: column

        x: (root.width - width) / 2
        y: {
            const free = root.availableHeight - implicitHeight;
            if (root.pinTop) {
                return Math.max(root.verticalMargin / 2, Math.min(root.topMax * root.unit, free));
            }
            return Math.max(root.verticalMargin / 2, root.compact ? Math.min(root.topMax * root.unit, free / 2) : free / 2);
        }
        width: root.columnWidth
        spacing: root.columnSpacing * root.unit
    }
}
