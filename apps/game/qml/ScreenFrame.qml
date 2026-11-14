// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts

// The frame of a screen that opens over another: the dotted page, a back button with the title beside it, and a body below.
// Settings, About, the parental gate and the text viewers use it, so their headers match and the back button does the same
// thing everywhere (the Escape key and the Android back key too).
//
// It covers whatever is underneath and takes every tap that misses a control. The body is a column of `contentWidth` in
// the middle of the window; `unit` scales the board's measurements (drawn on a 342 wide column) to it, and shrinks them
// again when the window is short.
Item {
    id: root

    property string title: ""
    property real titleSize: 48
    property real titleSpacing: 3
    property string backName: qsTr("Back")
    // How tall the content is at unit 1, so a short window shrinks it instead of cutting it off.
    property real designHeight: 760
    property real columnSpacing: 18
    // False while a viewer opened by this screen sits on top, so the Escape key goes to the viewer alone.
    property bool escapeEnabled: true

    // The boards are drawn on a column 342 wide. The column grows with the window up to 480, and shrinks, with all that is
    // in it, when the window is short.
    readonly property real unit: Math.max(0.6, Math.min((width - 48) / 342, 480 / 342, height / designHeight))
    readonly property real contentWidth: Math.min(width - 48, 342 * unit)
    readonly property alias body: bodyColumn

    default property alias content: bodyColumn.data

    signal backClicked

    focus: visible

    Keys.onBackPressed: event => {
        event.accepted = true;
        root.backClicked();
    }

    HalftoneBackground {
        anchors.fill: parent
    }

    // The screens underneath have controls of their own: this catches the taps that miss every control here.
    MouseArea {
        anchors.fill: parent
    }

    ColumnLayout {
        id: page

        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: Math.min(52, root.height * 0.065) * root.unit
        anchors.bottomMargin: Math.min(32, root.height * 0.04) * root.unit
        width: root.contentWidth
        spacing: root.columnSpacing * root.unit

        RowLayout {
            Layout.fillWidth: true
            spacing: 14 * root.unit

            ComicButton {
                objectName: "backButton"
                Layout.preferredWidth: 48 * root.unit
                Layout.preferredHeight: 48 * root.unit
                variant: "secondary"
                accessibleName: root.backName
                borderWidth: Theme.outlineSmall
                radius: Theme.radiusSmall
                onClicked: root.backClicked()

                BackIcon {
                    anchors.centerIn: parent
                    width: 22 * root.unit
                    height: 22 * root.unit
                }
            }

            Text {
                objectName: "screenTitle"
                Layout.fillWidth: true
                text: root.title
                elide: Text.ElideRight
                color: Theme.ink
                font.family: Theme.displayFont
                font.pixelSize: root.titleSize * root.unit
                font.letterSpacing: root.titleSpacing * root.unit
            }
        }

        ColumnLayout {
            id: bodyColumn

            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: root.columnSpacing * root.unit
        }
    }

    Shortcut {
        sequence: "Esc"
        // Not while another screen sits on top (the parental gate over Settings): the Escape key belongs to that one.
        enabled: root.visible && root.enabled && root.escapeEnabled
        onActivated: root.backClicked()
    }
}
