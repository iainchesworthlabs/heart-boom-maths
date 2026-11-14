// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick

// The hint button. It reads "HINT - 2 COINS" while a hint can be bought, goes grey when it cannot (too few coins, or the
// question already had one), and after use says what it did: "HINT USED - TWO TILES GONE".
ComicButton {
    id: root

    required property PlayMetrics metrics
    property int cost: 0
    property bool used: false
    property int removedCount: 0

    text: used ? qsTr("HINT USED · %1 TILES GONE").arg(Theme.countWord(removedCount)) : qsTr("HINT · %1 COINS").arg(cost)
    accessibleName: used ? qsTr("Hint used, %1 tiles gone").arg(Theme.countWord(removedCount).toLowerCase()) : (enabled ? qsTr("Hint, costs %1 coins").arg(cost) : qsTr("Hint, not available"))
    displayLabel: false
    hapticTap: false // the hint has its own haptic
    pixelSize: metrics.hintFont
    borderWidth: metrics.hintBorder
    radius: metrics.hintRadius
    horizontalPadding: metrics.hintPadding
    implicitHeight: metrics.hintHeight
}
