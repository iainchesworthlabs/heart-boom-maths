// SPDX-License-Identifier: GPL-3.0-or-later
#include "services/build_info.hpp"

#include "build_info_data.hpp"
#include "heart_boom_maths/profile.hpp"

namespace heart_boom_maths::game
{

BuildInfo BuildInfo::current()
{
    HBM_ZONE("build_info.current");
    return BuildInfo{
        .app_name = generated::BUILD_APP_NAME,
        .version = generated::BUILD_VERSION,
        .build_number = generated::BUILD_NUMBER,
        .commit = generated::BUILD_COMMIT,
        .branch = generated::BUILD_BRANCH,
        .date = generated::BUILD_DATE,
        .qt_version = generated::BUILD_QT_VERSION,
    };
}

std::string BuildInfo::footer() const
{
    return app_name + " " + version + " (build " + std::to_string(build_number) + ")";
}

} // namespace heart_boom_maths::game
