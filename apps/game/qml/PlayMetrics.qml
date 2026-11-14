// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick

// The measurements of the play screen in its three arrangements:
//
//   compact    a phone in portrait: stats stacked over the question, tiles in a 2 by 2 grid at the bottom
//   portrait   a tablet in portrait: the same arrangement scaled up
//   landscape  a desktop or browser window: the stats move to a left panel and the four tiles sit in one row
//
// One place for every number, so the screen's layout code reads as layout.
QtObject {
    id: metrics

    // "compact", "portrait" or "landscape".
    property string mode: "compact"

    function pick(compact, portrait, landscape) {
        return mode === "compact" ? compact : (mode === "portrait" ? portrait : landscape);
    }

    readonly property bool wide: mode === "landscape"

    // The page.
    readonly property real paddingTop: pick(52, 72, 36)
    readonly property real paddingSide: pick(20, 60, 56)
    readonly property real paddingBottom: pick(36, 56, 40)
    readonly property real gap: pick(16, 28, 24)
    readonly property real dotGrid: pick(12, 16, 14)
    readonly property real dotRadius: pick(1.6, 2.2, 2)

    // The top bar.
    readonly property real pauseSize: pick(48, 64, 56)
    readonly property real pauseBorder: pick(4, 5, 5)
    readonly property real pauseRadius: pick(12, 16, 14)
    readonly property real pauseIcon: pick(18, 24, 22)
    readonly property real levelFont: pick(28, 46, 46)
    readonly property real levelSpacing: pick(1, 2, 2)
    readonly property real countFont: pick(18, 30, 28)
    readonly property real endlessFont: pick(18, 24, 24)

    // The stats.
    readonly property real heartSize: pick(120, 190, 200)
    readonly property real rowGap: pick(14, 28, 18)
    readonly property real statGap: pick(8, 14, 12) // between the two chips
    readonly property real statRowGap: pick(8, 14, 18) // between the points and the chips
    readonly property real pointsFont: pick(26, 46, 46)
    readonly property real pointsLabelFont: pick(14, 22, 20)
    readonly property real pointsBorder: pick(4, 5, 5)
    // The room inside the points card (the desktop's points are plain text with 4 at the sides) and inside the chips.
    readonly property real pointsPadX: pick(12, 20, 4)
    readonly property real pointsPadY: pick(6, 10, 0)
    readonly property real badgePadY: pick(4, 6, 4)
    readonly property real pointsRadius: pick(12, 16, 16)
    readonly property real badgeFont: pick(24, 40, 32)
    readonly property real badgeBorder: pick(4, 5, 4)
    readonly property real badgeRadius: pick(12, 16, 14)
    readonly property real coinFont: pick(20, 34, 28)
    readonly property real coinSize: pick(18, 30, 24)
    readonly property real deltaFont: pick(15, 24, 20)

    // The fuse.
    readonly property real fuseHeight: pick(26, 40, 34)
    readonly property real fuseBorder: pick(4, 5, 5)
    readonly property real fuseZoneBorder: pick(3, 4, 4)
    readonly property real stripePeriod: pick(12, 16, 14)
    readonly property real starSize: pick(18, 28, 24)

    // The question card.
    readonly property real cardBorder: pick(5, 6, 6)
    readonly property real cardRadius: pick(22, 30, 30)
    readonly property real cardShadow: pick(8, 12, 12)
    readonly property real cardFont: pick(52, 104, 128)
    readonly property real cardFontMulti: pick(42, 84, 100)
    readonly property real slotWidth: pick(64, 100, 120)
    readonly property real slotHeight: pick(76, 118, 140)
    readonly property real slotBorder: pick(4, 6, 6)
    readonly property real slotRadius: pick(14, 22, 24)
    readonly property real partSpacing: pick(10, 16, 20)
    readonly property real cardPaddingY: pick(26, 40, 0)

    // The hint button and the messages.
    readonly property real hintHeight: pick(44, 60, 52)
    readonly property real hintFont: pick(17, 26, 20)
    readonly property real hintBorder: pick(4, 5, 4)
    readonly property real hintRadius: pick(22, 30, 26)
    readonly property real hintPadding: pick(18, 32, 18)
    readonly property real messageGap: pick(18, 28, 18)
    readonly property real bannerFont: pick(32, 44, 44)
    readonly property real bannerBorder: pick(4, 5, 5)
    readonly property real bannerShadow: pick(5, 7, 7)

    // The answer tiles.
    readonly property int tileColumns: wide ? 4 : 2
    readonly property real tileHeight: pick(96, 150, 150)
    readonly property real tileBorder: pick(5, 6, 6)
    readonly property real tileRadius: pick(20, 28, 28)
    readonly property real tileShadow: pick(6, 9, 9)
    readonly property real tileFont: pick(50, 76, 76)
    readonly property real tileGap: pick(16, 28, 24)

    // The left panel of the landscape arrangement.
    readonly property real panelWidth: 360
    readonly property real panelPadding: 24
    readonly property real panelGap: 18
    readonly property real columnGap: 32
    readonly property real panelBorder: 5
    readonly property real panelRadius: 22
    readonly property real panelShadow: 8
}
