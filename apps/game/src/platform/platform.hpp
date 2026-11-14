// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string_view>

namespace heart_boom_maths::game::platform
{

/// The operating system this build runs on. Every directory under src/platform/<os>/ implements the same declarations,
/// and CMake compiles the one that matches the target, so shared code never asks which system it is on. Lifecycle,
/// screen-time and storage services join this header as the platform work (M5) needs them.
[[nodiscard]] std::string_view platform_name() noexcept;

} // namespace heart_boom_maths::game::platform
