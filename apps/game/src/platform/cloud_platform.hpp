// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "services/cloud_store.hpp"

#include <memory>

namespace heart_boom_maths::game::platform
{

/// The cloud store of this operating system: the iCloud key-value store on iOS, NullCloudStore everywhere else. Like
/// score_platform.hpp, every directory under src/platform/<os>/ defines it and CMake compiles the one that matches the
/// target. Making the store contacts nothing (docs/ios.md).
[[nodiscard]] std::unique_ptr<CloudStore> make_cloud_store();

} // namespace heart_boom_maths::game::platform
