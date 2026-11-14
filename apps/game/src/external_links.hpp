// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace heart_boom_maths::game
{

/// The only web addresses the game can ever open, written once. About hands one of them to GameController::requestLink,
/// which asks a grown-up first; nothing else in the game names an address. Both are https, as requestLink requires.
inline constexpr const char* PRIVACY_POLICY_URL =
    "https://iainchesworthlabs.github.io/heart-boom-maths/privacy-policy/";
inline constexpr const char* REPOSITORY_URL = "https://github.com/iainchesworthlabs/heart-boom-maths";

} // namespace heart_boom_maths::game
