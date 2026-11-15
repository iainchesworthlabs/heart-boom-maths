// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts

// The title screen (boards CP1 and CP18): who is playing with their best score and coins, the heart bomb in its burst, the
// name, and the buttons. PLAY starts a game; when a run was saved, CONTINUE and NEW GAME stand in its place, with a line under them that
// says what the saved run was. TOP 10 and the gear (Settings) are said by signals, and the shell opens them.
//
// A CONTINUE that fails (the controller says so and stays here, the saved run gone) shows a short calm line where the
// saved-game line was. Nothing starts by itself.
//
// The player card (not on board CP1, which has a BEST pill and a coin pill) is one button across the top: the player's name
// and a down chevron, and under them BEST and the coins. It says WHO'S PLAYING by `playersRequested`. CONTINUE and its
// caption are the current player's: the controller shows them only while the saved run is theirs.
Item {
    id: root

    required property GameController game

    // The saved run's caption: its level and the time left.
    readonly property var saved: game.savedRun
    readonly property bool hasCaption: game.hasSavedRun && saved.level !== undefined
    // True once CONTINUE has failed on this screen.
    property bool continueFailed: false

    // The boards are drawn on a column 342 wide and 844 tall; the column grows with the window up to 480 and shrinks with
    // the height of it.
    readonly property real unit: Math.max(0.6, Math.min((width - 48) / 342, 480 / 342, height / 844))
    readonly property real contentWidth: Math.min(width - 48, 342 * unit)

    signal top10Requested
    signal settingsRequested
    signal playersRequested

    Connections {
        target: root.game

        function onContinueFailed() {
            root.continueFailed = true;
        }
    }

    HalftoneBackground {
        anchors.fill: parent
    }

    ColumnLayout {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        // The board's top margin less the 18 that the player card (24 taller than the pill it replaces) takes from it and
        // from the gap above the hero, so the hero stays where board CP1 has it.
        anchors.topMargin: (Math.min(56, root.height * 0.066) - 18) * root.unit
        anchors.bottomMargin: Math.min(36, root.height * 0.043) * root.unit
        width: root.contentWidth
        spacing: 16 * root.unit

        // Who is playing, with their best score and their coins: one card across the top and one button, which says
        // `playersRequested`. The name and a down chevron are on the first line, BEST and the coins under them. The card is
        // not on board CP1 (the board has the two pills); it is taller than they were, and the column gives the height back
        // from the margin above it and the gap below it, so nothing from the burst down moves.
        ComicButton {
            id: playerCard

            // The whole card is one button, so each line is inside the outline with this much room on every side.
            readonly property real sidePadding: 14 * root.unit
            readonly property real linePadding: 3 * root.unit

            objectName: "playerChip"
            Layout.fillWidth: true
            Layout.preferredHeight: 72 * root.unit
            variant: "secondary"
            accessibleName: qsTr("Playing as %1. Best score %2. %3 coins. Choose who is playing").arg(root.game.playerName).arg(Theme.formatNumber(root.game.bestScore)).arg(Theme.formatNumber(root.game.wallet))
            borderWidth: Theme.outlineSmall
            radius: 18
            shadowOffset: Theme.shadowSmall * root.unit
            onClicked: root.playersRequested()

            ColumnLayout {
                anchors.fill: parent
                anchors.leftMargin: playerCard.borderWidth + playerCard.sidePadding
                anchors.rightMargin: playerCard.borderWidth + playerCard.sidePadding
                anchors.topMargin: playerCard.borderWidth + playerCard.linePadding
                anchors.bottomMargin: playerCard.borderWidth + playerCard.linePadding
                spacing: 0

                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 8 * root.unit

                    Text {
                        objectName: "playerChipName"
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignVCenter
                        text: root.game.playerName
                        elide: Text.ElideRight
                        color: Theme.ink
                        font.family: Theme.displayFont
                        font.pixelSize: 26 * root.unit
                        font.letterSpacing: 1 * root.unit
                        Accessible.ignored: true
                    }
                    // The back chevron turned to point down: it says a choice opens.
                    ChevronIcon {
                        Layout.alignment: Qt.AlignVCenter
                        Layout.preferredWidth: 16 * root.unit
                        Layout.preferredHeight: 16 * root.unit
                        rotation: -90
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 8 * root.unit

                    Row {
                        objectName: "bestScore"
                        Layout.alignment: Qt.AlignVCenter
                        spacing: 8 * root.unit

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr("BEST")
                            color: Theme.ink
                            font.family: Theme.bodyFont
                            font.pixelSize: 16 * root.unit
                            Accessible.ignored: true
                        }
                        Text {
                            objectName: "bestScoreValue"
                            anchors.verticalCenter: parent.verticalCenter
                            text: Theme.formatNumber(root.game.bestScore)
                            color: Theme.ink
                            font.family: Theme.displayFont
                            font.pixelSize: 26 * root.unit
                            font.letterSpacing: 1 * root.unit
                            Accessible.ignored: true
                        }
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    CoinWallet {
                        objectName: "walletPill"
                        Layout.alignment: Qt.AlignVCenter
                        value: root.game.wallet
                        coinSize: 22 * root.unit
                        pixelSize: 26 * root.unit
                        displayNumerals: true
                        plain: true
                        Accessible.ignored: true
                    }
                }
            }
        }

        // The heart bomb in the red burst.
        Item {
            id: hero

            objectName: "hero"
            Layout.alignment: Qt.AlignHCenter
            // 6 off the column's 16 gap: the other part of the height the player card needs.
            Layout.topMargin: -6 * root.unit
            Layout.preferredWidth: 342 * root.unit
            Layout.preferredHeight: 262 * root.unit

            StarBurst {
                objectName: "heroBurst"
                x: 40 * root.unit
                y: 0
                width: 262 * root.unit
                height: 262 * root.unit
                outerColor: Theme.red
                outerOutline: 4 * root.unit
                innerOutline: 3 * root.unit
                spikes: 12
                valleyDepth: 0.74
                innerShare: 0.8
            }

            HeartBomb {
                objectName: "titleHeart"
                x: 97 * root.unit
                y: 52 * root.unit
                size: 150 * root.unit
                showSeconds: false
                face: true
            }
        }

        // The name on its tilted card.
        ComicCard {
            objectName: "titleCard"
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 4 * root.unit
            Layout.preferredWidth: titleLines.implicitWidth + 58 * root.unit
            Layout.preferredHeight: titleLines.implicitHeight + 18 * root.unit + 2 * borderWidth
            borderWidth: Theme.outlineLarge
            radius: 18
            shadowOffset: Theme.shadowLarge * root.unit
            rotation: -3
            Accessible.role: Accessible.Heading
            Accessible.name: qsTr("Heart Boom Maths")

            Column {
                id: titleLines

                anchors.centerIn: parent

                Text {
                    objectName: "titleLineOne"
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("HEART BOOM")
                    color: Theme.red
                    font.family: Theme.displayFont
                    font.pixelSize: 56 * root.unit
                    font.letterSpacing: 2 * root.unit
                    height: 53.2 * root.unit
                    verticalAlignment: Text.AlignVCenter
                }
                Text {
                    objectName: "titleLineTwo"
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("MATHS")
                    color: Theme.blue
                    font.family: Theme.displayFont
                    font.pixelSize: 56 * root.unit
                    font.letterSpacing: 2 * root.unit
                    height: 53.2 * root.unit
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        Text {
            objectName: "tagline"
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: qsTr("%1 sums. %2 seconds. Beat the bomb!").arg(root.game.firstLevelQuestions).arg(root.game.firstLevelSeconds)
            color: Theme.ink
            font.family: Theme.bodyFont
            font.pixelSize: 22 * root.unit
        }

        Item {
            Layout.fillHeight: true
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 14 * root.unit

            ComicButton {
                objectName: "continueButton"
                Layout.fillWidth: true
                Layout.preferredWidth: 0
                Layout.preferredHeight: 84 * root.unit
                visible: root.game.hasSavedRun
                variant: "primary"
                text: qsTr("CONTINUE")
                pixelSize: 32 * root.unit
                letterSpacing: 2
                borderWidth: Theme.outlineLarge
                radius: Theme.radiusLarge
                shadowOffset: Theme.shadowLarge * root.unit
                onClicked: root.game.continueGame()
            }

            ComicButton {
                objectName: "playButton"
                Layout.fillWidth: true
                Layout.preferredWidth: 0
                Layout.preferredHeight: 84 * root.unit
                variant: "danger"
                text: root.game.hasSavedRun ? qsTr("NEW GAME") : qsTr("PLAY")
                pixelSize: (root.game.hasSavedRun ? 32 : 52) * root.unit
                letterSpacing: root.game.hasSavedRun ? 2 : 3
                borderWidth: Theme.outlineLarge
                radius: Theme.radiusLarge
                shadowOffset: Theme.shadowLarge * root.unit
                onClicked: root.game.startGame()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 14 * root.unit

            ComicButton {
                objectName: "top10Button"
                Layout.fillWidth: true
                Layout.preferredHeight: 64 * root.unit
                variant: "secondary"
                accessibleName: qsTr("Top 10")
                borderWidth: Theme.outlineLarge
                radius: 18
                shadowOffset: Theme.shadowSmall * root.unit
                onClicked: root.top10Requested()

                Row {
                    anchors.centerIn: parent
                    spacing: 10 * root.unit

                    TrophyIcon {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 30 * root.unit
                        height: 30 * root.unit
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("TOP 10")
                        color: Theme.ink
                        font.family: Theme.displayFont
                        font.pixelSize: 34 * root.unit
                        font.letterSpacing: 2
                    }
                }
            }

            ComicButton {
                objectName: "settingsButton"
                Layout.preferredWidth: 64 * root.unit
                Layout.preferredHeight: 64 * root.unit
                variant: "secondary"
                accessibleName: qsTr("Settings")
                borderWidth: Theme.outlineLarge
                radius: 18
                shadowOffset: Theme.shadowSmall * root.unit
                onClicked: root.settingsRequested()

                GearIcon {
                    anchors.centerIn: parent
                    width: 32 * root.unit
                    height: 32 * root.unit
                }
            }
        }

        // One line under the buttons: the saved run, what went wrong with CONTINUE, or the plain promise.
        Text {
            objectName: "titleCaption"
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: {
                if (root.continueFailed) {
                    return qsTr("That saved game could not be opened. You can start a new one.");
                }
                if (root.hasCaption) {
                    return qsTr("Saved game: level %1, %2 left").arg(root.saved.level).arg(Theme.formatTime(root.saved.secondsLeft));
                }
                return qsTr("Every game starts at level 1");
            }
            color: Theme.ink
            font.family: Theme.bodyFont
            font.pixelSize: 16 * root.unit
        }
    }
}
