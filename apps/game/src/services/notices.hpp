// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

namespace heart_boom_maths::game
{

/// The third-party licence texts About shows: Qt, the dependencies and the two fonts. The text is a resource built at
/// configure time (cmake/HbmNotices.cmake), so it is empty only when the resource is missing from the build.
[[nodiscard]] QString notices_text();

} // namespace heart_boom_maths::game
