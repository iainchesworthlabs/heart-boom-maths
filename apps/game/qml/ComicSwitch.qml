// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick

// An on/off switch. The state is told three ways, so colour is never the only one: the word ON or OFF, the side the knob
// sits on, and the track (green or grey). It shows `checked` and does not change it: a tap only says `clicked`, and the
// screen decides, because some switches (Game Center) wait for a grown-up first.
//
// A screen reader hears the name and, as a check box, whether it is on. Space, Return and Enter work from the keyboard.
Item {
    id: root

    property bool checked: false
    property string accessibleName: ""

    readonly property bool pressed: tap.pressed

    signal clicked

    implicitWidth: 100
    implicitHeight: 50
    activeFocusOnTab: true

    Accessible.role: Accessible.CheckBox
    Accessible.name: root.accessibleName
    Accessible.checkable: true
    Accessible.checked: root.checked
    Accessible.description: root.checked ? qsTr("On") : qsTr("Off")
    Accessible.focusable: true
    Accessible.onToggleAction: if (root.enabled)
        root.clicked()
    Accessible.onPressAction: if (root.enabled)
        root.clicked()

    Keys.onPressed: event => {
        if (root.enabled && (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space)) {
            event.accepted = true;
            root.clicked();
        }
    }

    // The keyboard focus ring.
    Rectangle {
        anchors.fill: parent
        anchors.margins: -6
        visible: root.activeFocus
        radius: height / 2
        color: "transparent"
        border.width: 3
        border.color: Theme.ink
    }

    Rectangle {
        id: track

        anchors.fill: parent
        radius: height / 2
        color: root.checked ? Theme.green : Theme.switchOff
        border.width: Theme.outlineSmall
        border.color: Theme.ink
        opacity: root.enabled ? 1 : 0.5

        Text {
            objectName: "switchLabel"
            anchors.verticalCenter: parent.verticalCenter
            x: root.checked ? 12 : parent.width - width - 12
            text: root.checked ? qsTr("ON") : qsTr("OFF")
            color: Theme.ink
            font.family: Theme.displayFont
            font.pixelSize: 24
            font.letterSpacing: 1
        }

        Rectangle {
            id: knob

            objectName: "switchKnob"
            anchors.verticalCenter: parent.verticalCenter
            x: root.checked ? parent.width - width - 4 : 4
            width: parent.height - 8
            height: width
            radius: width / 2
            color: Theme.white
            border.width: Theme.outlineSmall
            border.color: Theme.ink
        }
    }

    TapHandler {
        id: tap

        enabled: root.enabled
        gesturePolicy: TapHandler.ReleaseWithinBounds
        onTapped: {
            root.clicked();
            // After the click, so the Haptics switch that has just turned itself on is felt, and one that has just turned
            // itself off is not.
            Haptics.tap();
        }
    }
}
