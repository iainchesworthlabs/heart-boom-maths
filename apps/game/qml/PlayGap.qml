// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts

// The space between two parts of the play column. It is `gap` high when the window has room and shrinks to `gapMin` when
// it has not (the alert's banner takes a row of its own), so no part has to overlap another.
Item {
    required property PlayMetrics metrics

    // A layout shrinks only the items that fill, so the gap fills but may not grow past `gap`.
    Layout.fillWidth: true
    Layout.fillHeight: true
    Layout.preferredHeight: metrics.gap
    Layout.minimumHeight: metrics.gapMin
    Layout.maximumHeight: metrics.gap
}
