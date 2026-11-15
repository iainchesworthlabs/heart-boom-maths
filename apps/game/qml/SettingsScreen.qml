// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts

// Settings (board CP14): the Sound effects and Music switches, the Haptics switch where the device can buzz, the Game Center
// switch where the device has a leaderboard, the iCloud sync switch where the device has an iCloud account, ABOUT, and the
// build line at the bottom. A switch writes through the controller, which saves it. The Game Center and iCloud sync
// switches turn off at once and turn on only after the parental gate.
ScreenFrame {
    id: root

    required property GameController game

    signal aboutRequested

    title: qsTr("SETTINGS")
    // The content is taller by one row where the device can make haptics, and by one more where it can sync, so a short
    // window shrinks it by that much more.
    designHeight: 640 + (root.game.hapticsAvailable ? 110 : 0) + (root.game.cloudSyncAvailable ? 120 : 0)

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

    // Only where the device can make a haptic (a phone, a browser on a touch screen); hidden on a desktop.
    SettingRow {
        objectName: "hapticsRow"
        Layout.fillWidth: true
        visible: root.game.hapticsAvailable
        unit: root.unit
        title: qsTr("Haptics")
        hint: qsTr("Feel it as you play")
        switchObjectName: "hapticsSwitch"
        checked: root.game.hapticsEnabled
        onToggleRequested: root.game.setHapticsEnabled(!root.game.hapticsEnabled)

        HapticsIcon {
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
        switchObjectName: "gameCenterSwitch"
        checked: root.game.gameCenterEnabled
        onToggleRequested: root.game.requestGameCenter(!root.game.gameCenterEnabled)

        TrophyIcon {
            anchors.fill: parent
        }
    }

    // Only on an Apple device that has an iCloud account signed in (the controller asks the cloud store, which does not
    // read the family's copy to say so). Like Game Center it turns off at once and turns on only after the parental gate.
    SettingRow {
        objectName: "cloudSyncRow"
        Layout.fillWidth: true
        visible: root.game.cloudSyncAvailable
        unit: root.unit
        title: qsTr("iCloud sync")
        hint: qsTr("Players on all devices\nNeeds a grown-up")
        switchObjectName: "cloudSyncSwitch"
        checked: root.game.cloudSyncEnabled
        onToggleRequested: root.game.requestCloudSync(!root.game.cloudSyncEnabled)

        CloudIcon {
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
