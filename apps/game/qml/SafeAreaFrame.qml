// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick

// The page and the content inside the part of the window that the device leaves free: the cut-out of a phone, its rounded
// corners, the home indicator and the system bars. Qt 6.9 and later draw a window edge to edge and report what to keep
// clear in `SafeArea.margins`; it is zero on a desktop window, so this frame costs nothing there. The halftone page runs
// under the unsafe edges so the window has no blank strip, and its dots are in step with the page inside.
Item {
    id: root

    /// What goes inside: the application shell. It is laid out inside the safe area.
    property alias content: inner.data

    /// The part of the window inside the safe area, in the frame's own coordinates.
    readonly property alias safeContent: inner

    /// What the device keeps clear on each side, for what is drawn over the whole window (AlertFrame).
    readonly property real unsafeLeft: SafeArea.margins.left
    readonly property real unsafeTop: SafeArea.margins.top
    readonly property real unsafeRight: SafeArea.margins.right
    readonly property real unsafeBottom: SafeArea.margins.bottom

    HalftoneBackground {
        objectName: "windowBackground"
        anchors.fill: parent
        originX: root.SafeArea.margins.left
        originY: root.SafeArea.margins.top
    }

    Item {
        id: inner

        objectName: "safeContent"
        anchors.fill: parent
        anchors.leftMargin: root.SafeArea.margins.left
        anchors.topMargin: root.SafeArea.margins.top
        anchors.rightMargin: root.SafeArea.margins.right
        anchors.bottomMargin: root.SafeArea.margins.bottom
    }
}
