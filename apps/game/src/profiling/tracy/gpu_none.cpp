// SPDX-License-Identifier: GPL-3.0-or-later
// The platforms without a Direct3D 11 context: no GPU zones, the GPU frame time plot only. src/platform and the
// profiling directory work the same way: CMake compiles this file everywhere but Windows, where gpu_d3d11.cpp is
// compiled instead.
#include "gpu_context.hpp"

namespace heart_boom_maths::game::profiling
{

GpuContext* create_d3d11_context(void* /*device*/, void* /*device_context*/) noexcept
{
    return nullptr;
}

} // namespace heart_boom_maths::game::profiling
