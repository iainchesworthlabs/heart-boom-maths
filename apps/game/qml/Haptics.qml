// SPDX-License-Identifier: GPL-3.0-or-later
pragma Singleton
import QtQuick

// The one way a control asks for its light tick. ComicButton, ComicSwitch and the name keys call tap(), so a screen writes
// nothing for it and the rules (switch off, paused, too soon after another) stay in the controller's haptics service. The
// shell gives it the controller; a control used alone, as in the component tests, finds none and does nothing.
QtObject {
    id: root

    property GameController controller: null

    function tap() {
        if (root.controller !== null) {
            root.controller.tap();
        }
    }
}
