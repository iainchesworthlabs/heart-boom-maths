// SPDX-License-Identifier: GPL-3.0-or-later
#include "services/audio_backend.hpp"

namespace heart_boom_maths::game
{

// The build found no Qt Multimedia, so the game starts silent. See apps/game/CMakeLists.txt.
std::unique_ptr<AudioBackend> make_device_audio_backend()
{
    return nullptr;
}

} // namespace heart_boom_maths::game
