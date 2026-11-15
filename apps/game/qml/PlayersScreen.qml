// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Shapes

// WHO'S PLAYING: a page over the title that lists the players of this device, picks one, adds one, and leads to the name
// screen and the parental gate. There is no board for it yet (DECISIONS, P8): it is built from the components the boards
// already use.
//
// Each player is a white card with the name, the best score and the coins; the one playing is blue, with a tick and the word
// PLAYING, so it is marked by shape and words and not by colour alone. Tapping a card makes that player current. NAME or
// RENAME opens the name screen (the shell does that, by `nameRequested`), and REMOVE asks the grown-up first: the gate is the
// controller's, and it offers no REMOVE for the last player. ADD PLAYER shows while the controller says there is room, and
// leads to the name screen for the new player, who is already current.
//
// Nothing here works out a rule. The list, the room for another player and the refusals are the controller's.
ScreenFrame {
    id: root

    required property GameController game

    // The name screen is wanted for a player; `isNew` for one that has just been added.
    signal nameRequested(string playerId, bool isNew)

    title: qsTr("WHO'S PLAYING")
    titleSize: 40
    titleSpacing: 2
    // A full list (maxPlayers cards) and the heading, so a short window shrinks it all rather than cutting it off.
    designHeight: 230 + 140 * root.game.maxPlayers
    columnSpacing: 12

    Repeater {
        model: root.game.players

        Item {
            id: row

            required property int index
            required property var modelData

            readonly property bool current: modelData.current
            readonly property color textColor: current ? Theme.white : Theme.ink

            objectName: "playerCard" + index
            Layout.fillWidth: true
            Layout.preferredHeight: 140 * root.unit

            // The whole card selects the player. The two buttons below are its siblings, drawn over it, so a tap on one of
            // them never reaches the card.
            ComicButton {
                id: card

                objectName: "playerSelect" + row.index
                anchors.fill: parent
                enabled: root.game.canChangePlayers
                variant: row.current ? "primary" : "secondary"
                accessibleName: (row.current ? qsTr("%1, playing now.") : qsTr("Play as %1.")).arg(row.modelData.displayName) + " " + qsTr("Best score %1, %2 coins.").arg(Theme.formatNumber(row.modelData.best)).arg(Theme.formatNumber(row.modelData.coins))
                borderWidth: Theme.outlineSmall
                radius: 18 * root.unit
                shadowOffset: 4 * root.unit
                onClicked: root.game.selectPlayer(row.modelData.id)

                // A tick in a ring for the player who is playing and an empty ring for the others: the same tick the right
                // answer wears.
                Shape {
                    objectName: "playerTick" + row.index
                    x: 14 * root.unit
                    y: 12 * root.unit
                    width: 30
                    height: 30
                    scale: root.unit
                    transformOrigin: Item.TopLeft
                    preferredRendererType: Shape.CurveRenderer

                    ShapePath {
                        fillColor: Theme.white
                        strokeColor: Theme.ink
                        strokeWidth: 3

                        PathSvg {
                            path: "M3 15 A12 12 0 1 0 27 15 A12 12 0 1 0 3 15 Z"
                        }
                    }
                    ShapePath {
                        fillColor: "transparent"
                        strokeColor: Theme.ink
                        strokeWidth: row.current ? 3.5 : 0
                        capStyle: ShapePath.RoundCap
                        joinStyle: ShapePath.RoundJoin

                        PathSvg {
                            path: "M9 15 L13 19 L21 11"
                        }
                    }
                }

                Text {
                    objectName: "playerName" + row.index
                    x: 54 * root.unit
                    y: 6 * root.unit
                    width: card.width - x - (row.current ? playingTag.width + 24 * root.unit : 14 * root.unit)
                    text: row.modelData.displayName
                    elide: Text.ElideRight
                    color: row.textColor
                    font.family: Theme.displayFont
                    font.pixelSize: 32 * root.unit
                    font.letterSpacing: 2 * root.unit
                }

                Badge {
                    id: playingTag

                    objectName: "playingTag" + row.index
                    anchors.right: parent.right
                    anchors.rightMargin: 14 * root.unit
                    y: 14 * root.unit
                    visible: row.current
                    text: qsTr("PLAYING")
                    fillColor: Theme.background
                    borderWidth: Math.max(2, 3 * root.unit)
                    radius: 8 * root.unit
                    pixelSize: 16 * root.unit
                    letterSpacing: 1
                    horizontalPadding: 8 * root.unit
                }

                // The best score and the coins, the way the title says them: a small word and the number in the heading font.
                Row {
                    x: 54 * root.unit
                    y: 44 * root.unit
                    spacing: 20 * root.unit

                    Row {
                        spacing: 6 * root.unit

                        Text {
                            y: bestValue.baselineOffset - baselineOffset
                            text: qsTr("BEST")
                            color: row.textColor
                            font.family: Theme.bodyFont
                            font.pixelSize: 16 * root.unit
                        }
                        Text {
                            id: bestValue

                            objectName: "playerBest" + row.index
                            text: Theme.formatNumber(row.modelData.best)
                            color: row.textColor
                            font.family: Theme.displayFont
                            font.pixelSize: 26 * root.unit
                            font.letterSpacing: 1 * root.unit
                        }
                    }
                    Row {
                        spacing: 6 * root.unit

                        Text {
                            y: coinsValue.baselineOffset - baselineOffset
                            text: qsTr("COINS")
                            color: row.textColor
                            font.family: Theme.bodyFont
                            font.pixelSize: 16 * root.unit
                        }
                        Text {
                            id: coinsValue

                            objectName: "playerCoins" + row.index
                            text: Theme.formatNumber(row.modelData.coins)
                            color: row.textColor
                            font.family: Theme.displayFont
                            font.pixelSize: 26 * root.unit
                            font.letterSpacing: 1 * root.unit
                        }
                    }
                }
            }

            RowLayout {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.leftMargin: 14 * root.unit
                anchors.rightMargin: 14 * root.unit
                anchors.bottomMargin: 12 * root.unit
                spacing: 10 * root.unit

                ComicButton {
                    objectName: "renameButton" + row.index
                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                    Layout.preferredHeight: Math.max(44, 48 * root.unit)
                    variant: "secondary"
                    text: row.modelData.named ? qsTr("RENAME") : qsTr("NAME")
                    accessibleName: (row.modelData.named ? qsTr("Rename %1") : qsTr("Give %1 a name")).arg(row.modelData.displayName)
                    pixelSize: 22 * root.unit
                    letterSpacing: 1
                    borderWidth: Theme.outlineSmall
                    radius: 14 * root.unit
                    shadowOffset: 0
                    onClicked: root.nameRequested(row.modelData.id, false)
                }

                // The last player cannot be removed, so the button is not there to be tapped.
                ComicButton {
                    objectName: "removeButton" + row.index
                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                    Layout.preferredHeight: Math.max(44, 48 * root.unit)
                    visible: root.game.canRemovePlayer
                    enabled: root.game.canChangePlayers
                    variant: "secondary"
                    text: qsTr("REMOVE")
                    accessibleName: qsTr("Remove %1, asks a grown-up first").arg(row.modelData.displayName)
                    pixelSize: 22 * root.unit
                    letterSpacing: 1
                    borderWidth: Theme.outlineSmall
                    radius: 14 * root.unit
                    shadowOffset: 0
                    onClicked: root.game.requestRemovePlayer(row.modelData.id)
                }
            }
        }
    }

    // While there is room (maxPlayers is the tuning's, never a number written here).
    RowButton {
        objectName: "addPlayerButton"
        Layout.fillWidth: true
        Layout.preferredHeight: 68 * root.unit
        visible: root.game.canAddPlayer
        enabled: root.game.canChangePlayers
        unit: root.unit
        label: qsTr("ADD PLAYER")
        arrow: true
        variant: "primary"
        pixelSize: 36 * root.unit
        letterSpacing: 3 * root.unit
        borderWidth: Theme.outlineLarge
        radius: 20
        shadowOffset: Theme.shadowSmall
        horizontalPadding: 22
        onClicked: {
            const id = root.game.addPlayer();
            if (id !== "") {
                root.nameRequested(id, true);
            }
        }
    }

    Item {
        Layout.fillHeight: true
    }
}
