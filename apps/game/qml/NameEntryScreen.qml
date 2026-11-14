// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts

// The name screen (board CP11), shown when a finished game's score earned a place in the top 10: the rank, the score and the
// level reached, a field for the name, SAVE and SKIP.
//
// What a name may hold is the core's rule, reached through the controller (normaliseName, canSaveName, nameMaxLength): this
// screen holds no letter set and no length. A hardware keyboard types into the field, and so does the on-screen keyboard,
// which shows on touch devices; the system keyboard is never asked for, so a name only ever holds what the core accepts.
//
// SAVE needs at least one letter. SKIP drops the entry. Both leave the screen by changing the controller, which the shell
// follows.
Item {
    id: root

    required property GameController game

    // The name as typed so far: capitals, A to Z and spaces, at most game.nameMaxLength.
    property string name: ""
    // "auto" shows the on-screen keyboard on a phone-sized window and on iOS and Android, and anywhere a finger has touched
    // the screen; "on" and "off" force it (a test, or a setting later).
    property string keyboardMode: "auto"
    // True once a touch has been seen on this screen.
    property bool touchSeen: false

    readonly property bool compact: Theme.sizeClassFor(width) === "compact"
    readonly property bool keyboardShown: keyboardMode === "on" || (keyboardMode === "auto" && (compact || touchSeen || Qt.platform.os === "ios" || Qt.platform.os === "android"))
    readonly property bool canSave: game.canSaveName(name)
    readonly property bool full: name.length >= game.nameMaxLength
    readonly property real keyboardHeight: keyboardShown ? Math.min(336, height * 0.4) : 0

    // Emitted just before the name is saved, so the shell can set up the Top 10 screen before the controller leaves the name
    // entry state.
    signal saveStarted

    function type_text(text) {
        root.name = root.game.normaliseName(root.name + text);
    }

    function delete_last() {
        root.name = root.name.slice(0, -1);
    }

    function save() {
        if (!root.canSave) {
            return;
        }
        root.saveStarted();
        root.game.saveName(root.name.trim());
    }

    function skip() {
        root.game.skipName();
    }

    focus: true
    Component.onCompleted: forceActiveFocus()

    Keys.onPressed: event => {
        if (event.key === Qt.Key_Backspace) {
            event.accepted = true;
            root.delete_last();
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            event.accepted = true;
            root.save();
        } else if (event.text.length === 1 && event.text >= " " && (event.modifiers & (Qt.ControlModifier | Qt.MetaModifier)) === 0) {
            // A printable key goes through the core's rule, which drops what a name may not hold.
            event.accepted = true;
            root.type_text(event.text);
        }
    }

    // A finger on the screen means the keyboard is wanted, even in a wide window.
    PointHandler {
        acceptedDevices: PointerDevice.TouchScreen
        onActiveChanged: {
            if (active) {
                root.touchSeen = true;
            }
        }
    }

    ScorePage {
        id: page

        anchors.fill: parent
        designHeight: 460
        columnSpacing: 12
        topMax: 44
        reservedBottom: root.keyboardHeight

        StarBurst {
            objectName: "nameBurst"
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: page.columnWidth
            Layout.preferredHeight: 130 * page.unit
            outerColor: Theme.orange
            text: qsTr("TOP 10!")
            textSize: 50 * page.unit
            letterSpacing: 3 * page.unit
        }

        ComicCard {
            objectName: "entryCard"
            Layout.fillWidth: true
            Layout.preferredHeight: rows.implicitHeight + 2 * (10 * page.unit) + 2 * borderWidth
            borderWidth: Theme.outlineLarge * page.unit
            radius: Theme.radiusLarge * page.unit
            shadowOffset: Theme.shadowLarge * page.unit

            ColumnLayout {
                id: rows

                anchors.fill: parent
                anchors.leftMargin: 20 * page.unit
                anchors.rightMargin: 20 * page.unit
                anchors.topMargin: 10 * page.unit
                anchors.bottomMargin: 10 * page.unit
                spacing: 4 * page.unit

                StatRow {
                    objectName: "rankRow"
                    Layout.fillWidth: true
                    pixelSize: 20 * page.unit
                    label: qsTr("RANK")
                    value: qsTr("%1 of %2").arg(root.game.nameEntryRank).arg(root.game.top10Size)
                }

                StatRow {
                    objectName: "entryScoreRow"
                    Layout.fillWidth: true
                    pixelSize: 20 * page.unit
                    label: qsTr("SCORE")
                    value: Theme.formatNumber(root.game.finalScore)
                }

                StatRow {
                    objectName: "entryLevelRow"
                    Layout.fillWidth: true
                    pixelSize: 20 * page.unit
                    label: qsTr("LEVEL REACHED")
                    value: String(root.game.finalLevel)
                }
            }
        }

        Text {
            objectName: "nameLabel"
            Layout.fillWidth: true
            Layout.bottomMargin: -6 * page.unit
            text: qsTr("TYPE YOUR NAME")
            color: Theme.ink
            font.family: Theme.displayFont
            font.pixelSize: 28 * page.unit
            font.letterSpacing: 2 * page.unit
        }

        // The field. It is drawn, not a TextInput: the keys feed it, and so does a hardware keyboard.
        ComicCard {
            id: field

            objectName: "nameField"
            Layout.fillWidth: true
            Layout.preferredHeight: 68 * page.unit
            borderWidth: Theme.outlineLarge * page.unit
            radius: 18 * page.unit
            shadowOffset: Theme.shadowSmall * page.unit

            Accessible.role: Accessible.EditableText
            Accessible.name: qsTr("Your name")
            Accessible.description: root.name
            Accessible.editable: true

            Text {
                id: nameText

                objectName: "nameText"
                anchors.left: parent.left
                anchors.leftMargin: 18 * page.unit
                anchors.right: counter.left
                anchors.rightMargin: 8 * page.unit
                anchors.verticalCenter: parent.verticalCenter
                text: root.name
                elide: Text.ElideRight
                color: Theme.ink
                font.family: Theme.displayFont
                font.pixelSize: 40 * page.unit
                font.letterSpacing: 4 * page.unit
            }

            // The place the next letter goes. It blinks, and it is a bar in the ink colour, so it does not depend on colour.
            Rectangle {
                id: caret

                objectName: "nameCaret"
                x: nameText.x + Math.min(nameText.contentWidth, nameText.width) + 2 * page.unit
                anchors.verticalCenter: parent.verticalCenter
                width: 4 * page.unit
                height: 40 * page.unit
                radius: 1
                color: Theme.ink
                visible: !root.full

                SequentialAnimation on opacity {
                    loops: Animation.Infinite
                    NumberAnimation {
                        to: 0
                        duration: 450
                    }
                    NumberAnimation {
                        to: 1
                        duration: 450
                    }
                }
            }

            Text {
                id: counter

                objectName: "nameCounter"
                anchors.right: parent.right
                anchors.rightMargin: 16 * page.unit
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("%1 / %2").arg(root.name.length).arg(root.game.nameMaxLength)
                color: Theme.ink
                font.family: Theme.bodyFont
                font.pixelSize: 18 * page.unit
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 14 * page.unit

            ComicButton {
                objectName: "saveButton"
                Layout.fillWidth: true
                Layout.preferredWidth: 0
                Layout.horizontalStretchFactor: 14
                Layout.preferredHeight: 64 * page.unit
                enabled: root.canSave
                variant: "danger"
                text: qsTr("SAVE")
                accessibleName: root.canSave ? qsTr("Save") : qsTr("Save, type a name first")
                pixelSize: 38 * page.unit
                letterSpacing: 3 * page.unit
                borderWidth: Theme.outlineLarge * page.unit
                radius: 18 * page.unit
                shadowOffset: Theme.shadowSmall * page.unit
                onClicked: root.save()
            }

            ComicButton {
                objectName: "skipButton"
                Layout.fillWidth: true
                Layout.preferredWidth: 0
                Layout.horizontalStretchFactor: 10
                Layout.preferredHeight: 64 * page.unit
                variant: "secondary"
                text: qsTr("SKIP")
                pixelSize: 34 * page.unit
                letterSpacing: 2 * page.unit
                borderWidth: Theme.outlineLarge * page.unit
                radius: 18 * page.unit
                shadowOffset: Theme.shadowSmall * page.unit
                onClicked: root.skip()
            }
        }
    }

    NameKeyboard {
        objectName: "nameKeyboard"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: root.keyboardHeight
        visible: root.keyboardShown
        lettersEnabled: !root.full
        onLetterTyped: letter => root.type_text(letter)
        onDeleteTyped: root.delete_last()
    }
}
