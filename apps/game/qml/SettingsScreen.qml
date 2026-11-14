// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts

// Settings (board CP14): the Sound effects and Music switches, the Game Center switch where the device has a leaderboard,
// ABOUT, and the build line at the bottom. A switch writes through the controller, which saves it. The Game Center switch
// turns off at once and turns on only after the parental gate.
ScreenFrame {
    id: root

    required property GameController game

    signal aboutRequested

    title: qsTr("SETTINGS")
    designHeight: 640

    SettingRow {
        objectName: "soundRow"
        Layout.fillWidth: true
        unit: root.unit
        title: qsTr("Sound effects")
        hint: qsTr("Chimes, buzzes, boom")
        switchObjectName: "soundSwitch"
        checked: root.game.soundEnabled
        onToggleRequested: root.game.setSoundEnabled(!root.game.soundEnabled)

        SoundIcon {
            anchors.fill: parent
        }
    }

    SettingRow {
        objectName: "musicRow"
        Layout.fillWidth: true
        unit: root.unit
        title: qsTr("Music")
        hint: qsTr("Tune while you play")
        switchObjectName: "musicSwitch"
        checked: root.game.musicEnabled
        onToggleRequested: root.game.setMusicEnabled(!root.game.musicEnabled)

        MusicIcon {
            anchors.fill: parent
        }
    }

    // Only where the device has a leaderboard; hidden everywhere else, and a hidden item takes no room in the layout.
    SettingRow {
        objectName: "gameCenterRow"
        Layout.fillWidth: true
        visible: root.game.gameCenterAvailable
        unit: root.unit
        title: qsTr("Game Center")
        hint: qsTr("Needs a grown-up")
        iconSize: 30
        switchObjectName: "gameCenterSwitch"
        checked: root.game.gameCenterEnabled
        onToggleRequested: root.game.requestGameCenter(!root.game.gameCenterEnabled)

        TrophyIcon {
            anchors.fill: parent
        }
    }

    RowButton {
        objectName: "aboutButton"
        Layout.fillWidth: true
        Layout.preferredHeight: 68 * root.unit
        unit: root.unit
        label: qsTr("ABOUT")
        arrow: true
        variant: "primary"
        pixelSize: 38 * root.unit
        letterSpacing: 3 * root.unit
        borderWidth: Theme.outlineLarge
        radius: 20
        shadowOffset: Theme.shadowSmall
        horizontalPadding: 22
        onClicked: root.aboutRequested()
    }

    Item {
        Layout.fillHeight: true
    }

    Text {
        objectName: "buildFooter"
        Layout.fillWidth: true
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        text: root.game.versionText
        color: Theme.ink
        font.family: Theme.bodyFont
        font.pixelSize: 16 * root.unit
    }
}
