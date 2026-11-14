// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts

// The top 10 (board CP12): the best scores kept on this device, each with its rank, name and score. `highlightRank` marks the
// entry that has just been saved; 0 marks none. With no entries it says so and invites a game. The back arrow and the play
// button are the shell's to act on, because where they lead depends on where the list was opened from.
ScorePage {
    id: root

    required property GameController game
    // The place to mark as new, 0 for none. The shell passes the controller's highlightRank when this screen follows a game,
    // and 0 when it is opened from the title.
    property int highlightRank: 0
    // The big red button: PLAY AGAIN after a game, PLAY from the title.
    property string playText: qsTr("PLAY AGAIN")

    readonly property bool empty: game.top10.length === 0

    signal backRequested
    signal playRequested

    designHeight: 804
    columnSpacing: 14
    topMax: 52
    // The play button stands at the foot of the window, 32 above its edge, as on the board, with the list above it.
    pinTop: true
    pinBottom: 32

    RowLayout {
        Layout.fillWidth: true
        spacing: 14 * root.unit

        ComicButton {
            objectName: "backButton"
            Layout.preferredWidth: 48 * root.unit
            Layout.preferredHeight: 48 * root.unit
            variant: "secondary"
            accessibleName: qsTr("Back")
            borderWidth: Theme.outlineSmall * root.unit
            radius: Theme.radiusSmall * root.unit
            onClicked: root.backRequested()

            ChevronIcon {
                anchors.centerIn: parent
                width: 22 * root.unit
                height: 22 * root.unit
            }
        }

        Text {
            objectName: "top10Title"
            Layout.fillWidth: true
            text: qsTr("TOP 10")
            color: Theme.ink
            font.family: Theme.displayFont
            font.pixelSize: 48 * root.unit
            font.letterSpacing: 3 * root.unit
            lineHeight: 1
        }
    }

    ColumnLayout {
        id: list

        objectName: "top10List"
        Layout.fillWidth: true
        visible: !root.empty
        spacing: 8 * root.unit

        Repeater {
            model: root.game.top10

            ScoreRow {
                id: entry

                required property var modelData

                objectName: "scoreRow" + modelData.rank
                Layout.fillWidth: true
                Layout.preferredHeight: 50 * root.unit
                unit: root.unit
                rank: modelData.rank
                name: modelData.name
                scoreText: Theme.formatNumber(modelData.score)
                highlighted: modelData.rank === root.highlightRank
            }
        }
    }

    ComicCard {
        objectName: "emptyCard"
        Layout.fillWidth: true
        Layout.preferredHeight: 150 * root.unit
        visible: root.empty
        borderWidth: Theme.outlineLarge * root.unit
        radius: Theme.radiusLarge * root.unit
        shadowOffset: Theme.shadowLarge * root.unit

        ColumnLayout {
            anchors.centerIn: parent
            width: parent.width - 40 * root.unit
            spacing: 6 * root.unit

            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("NO SCORES YET")
                color: Theme.ink
                font.family: Theme.displayFont
                font.pixelSize: 38 * root.unit
                font.letterSpacing: 2 * root.unit
            }

            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: qsTr("Play a game and be the first one here")
                color: Theme.ink
                font.family: Theme.bodyFont
                font.pixelSize: 20 * root.unit
            }
        }
    }

    Item {
        Layout.fillHeight: true
    }

    ComicButton {
        objectName: "top10PlayButton"
        Layout.fillWidth: true
        Layout.preferredHeight: 72 * root.unit
        variant: "danger"
        text: root.playText
        pixelSize: 44 * root.unit
        letterSpacing: 3 * root.unit
        borderWidth: Theme.outlineLarge * root.unit
        radius: Theme.radiusLarge * root.unit
        shadowOffset: Theme.shadowLarge * root.unit
        onClicked: root.playRequested()
    }

    Shortcut {
        sequence: "Esc"
        onActivated: root.backRequested()
    }
}
