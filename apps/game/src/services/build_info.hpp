// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>

namespace heart_boom_maths::game
{

/// What About and the Settings footer show about this build. The values are generated at configure time
/// (cmake/HbmBuildInfo.cmake). A local build reads build 0, and "dev" for the branch and the date.
struct BuildInfo
{
    std::string app_name;
    std::string version;
    int build_number = 0;
    std::string commit; // short hash, or "dev" when git was not there
    std::string branch; // branch or tag
    std::string date;
    std::string qt_version;

    [[nodiscard]] static BuildInfo current();

    /// The Settings footer: "Heart Boom Maths 1.0.0 (build 42)".
    [[nodiscard]] std::string footer() const;
};

} // namespace heart_boom_maths::game
