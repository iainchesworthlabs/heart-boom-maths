// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <span>
#include <string_view>

namespace heart_boom_maths::game::profiling
{

/// What the renderer of the scene graph says it drew in the frames since the last take.
struct SceneGraphCounts
{
    bool known = false;   // false when the log was not switched on (--profile-sg-stats)
    int opaque_nodes = 0; // of the last frame
    int opaque_batches = 0;
    int alpha_nodes = 0;
    int alpha_batches = 0;
    int max_nodes = 0;   // the most nodes (opaque and alpha) in any frame of the span
    int max_batches = 0; // the most batches (draw calls, near enough) in any frame of the span

    [[nodiscard]] int nodes() const noexcept
    {
        return opaque_nodes + alpha_nodes;
    }
    [[nodiscard]] int batches() const noexcept
    {
        return opaque_batches + alpha_batches;
    }
};

/// One line of the scene graph's own log, read into numbers. Qt does not expose the nodes and batches a frame has any
/// other way than by logging them (QSG_RENDERER_DEBUG=render), so the profiling build reads that log. The format is
/// Qt's and not a contract: a line this does not recognise is ignored and the figure stays as it was.
struct SceneGraphLine
{
    enum class Kind
    {
        NONE,
        PASSES,   // one message with two lines: " -> Opaque: 2 nodes in 2 batches..." and " -> Alpha: 46 nodes in 35
                  // batches..."
        RENDERER, // "time in renderer: total=17ms, preprocess=3, updates=0, rendering=14"
        FRAME,    // "[window ...][render thread ...] syncAndRender: frame rendered in 6ms, sync=1, render=4, swap=1"
    };
    Kind kind = Kind::NONE;
    int a = 0; // opaque nodes, or the renderer's total, or the frame's total
    int b = 0; // opaque batches, or preprocess, or sync
    int c = 0; // alpha nodes, or updates, or render
    int d = 0; // alpha batches, or rendering, or swap
};

[[nodiscard]] SceneGraphLine parse_scene_graph_line(std::string_view text) noexcept;

/// The counts since the last call.
[[nodiscard]] SceneGraphCounts take_scene_graph_counts();

} // namespace heart_boom_maths::game::profiling
