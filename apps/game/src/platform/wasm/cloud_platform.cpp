// SPDX-License-Identifier: GPL-3.0-or-later
#include "platform/cloud_platform.hpp"

namespace heart_boom_maths::game::platform
{

std::unique_ptr<CloudStore> make_cloud_store()
{
    return std::make_unique<NullCloudStore>();
}

} // namespace heart_boom_maths::game::platform
