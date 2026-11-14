// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "game_controller.hpp"

#include <QtQml/qqmlregistration.h>

namespace heart_boom_maths::game
{

/// Makes GameController known to the QML tools (qmllint, qmlcachegen) and to the screens as a type, without putting QML
/// macros into the controller's own header. The application builds the controller and hands it to the root item as an
/// initial property, so QML never creates one.
struct GameControllerForeign
{
    Q_GADGET
    QML_FOREIGN(GameController)
    QML_NAMED_ELEMENT(GameController)
    QML_UNCREATABLE("The application creates the controller and hands it to the main window.")
};

} // namespace heart_boom_maths::game
