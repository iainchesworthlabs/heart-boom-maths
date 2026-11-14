// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts

// The win screen (board CP8): the bonus the win paid, the wallet, then KEEP GOING (endless) or FINISH. The bonus is paid
// once; when an earlier game paid it the core's GAME_WON event carries 0 coins, and the card says so instead of showing a
// payment that did not happen.
ScorePage {
    id: root

    required property GameController game

    readonly property bool bonusPaid: game.winBonus > 0

    designHeight: 690
    columnSpacing: 16
    topMax: 56

    StarBurst {
        objectName: "winBurst"
        Layout.alignment: Qt.AlignHCenter
        Layout.preferredWidth: 252 * root.unit
        Layout.preferredHeight: 252 * root.unit
        outerColor: Theme.orange
        textColor: Theme.blue
        text: qsTr("YOU\nWIN!")
        textSize: 54 * root.unit
        letterSpacing: 2 * root.unit
        textRotation: -3
    }

    Text {
        objectName: "winHeading"
        Layout.fillWidth: true
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        text: qsTr("You cleared all %1 levels!").arg(root.game.campaignLevels)
        color: Theme.ink
        font.family: Theme.bodyFont
        font.pixelSize: 22 * root.unit
    }

    CoinCard {
        objectName: "bonusCard"
        Layout.fillWidth: true
        centered: true
        coinSize: 40 * root.unit
        pixelSize: (root.bonusPaid ? 46 : 34) * root.unit
        borderWidth: Theme.outlineLarge * root.unit
        radius: Theme.radiusLarge * root.unit
        shadowOffset: Theme.shadowLarge * root.unit
        horizontalPadding: 20 * root.unit
        verticalPadding: 14 * root.unit
        text: root.bonusPaid ? qsTr("+%1 COINS").arg(Theme.formatNumber(root.game.winBonus)) : qsTr("BONUS WON BEFORE")
    }

    Text {
        objectName: "winWallet"
        Layout.fillWidth: true
        horizontalAlignment: Text.AlignHCenter
        text: qsTr("Wallet %1").arg(Theme.formatNumber(root.game.wallet))
        color: Theme.ink
        font.family: Theme.bodyFont
        font.pixelSize: 18 * root.unit
    }

    ComicButton {
        objectName: "keepGoingButton"
        Layout.fillWidth: true
        Layout.preferredHeight: 76 * root.unit
        text: qsTr("KEEP GOING · ENDLESS")
        pixelSize: 36 * root.unit
        letterSpacing: 2 * root.unit
        borderWidth: Theme.outlineLarge * root.unit
        radius: Theme.radiusLarge * root.unit
        shadowOffset: Theme.shadowLarge * root.unit
        onClicked: root.game.keepGoing()
    }

    ComicButton {
        objectName: "finishButton"
        Layout.fillWidth: true
        Layout.preferredHeight: 60 * root.unit
        variant: "secondary"
        text: qsTr("FINISH")
        pixelSize: 32 * root.unit
        letterSpacing: 2 * root.unit
        borderWidth: Theme.outlineLarge * root.unit
        radius: Theme.radiusLarge * root.unit
        shadowOffset: Theme.shadowSmall * root.unit
        onClicked: root.game.finish()
    }

    Text {
        objectName: "endlessNote"
        Layout.fillWidth: true
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        text: qsTr("Endless levels get shorter and harder until the bomb goes off")
        color: Theme.ink
        font.family: Theme.bodyFont
        font.pixelSize: 16 * root.unit
    }
}
