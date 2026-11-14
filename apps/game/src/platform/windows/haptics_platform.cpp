// SPDX-License-Identifier: GPL-3.0-or-later
#include "platform/haptics_platform.hpp"

namespace heart_boom_maths::game::platform
{

std::unique_ptr<HapticBackend> make_haptic_backend()
{
    return std::make_unique<NullHapticBackend>();
}

} // namespace heart_boom_maths::game::platform
