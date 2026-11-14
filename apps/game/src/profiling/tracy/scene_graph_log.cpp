// SPDX-License-Identifier: GPL-3.0-or-later
#include "scene_graph_log.hpp"

#include "profiling.hpp"

#include <QByteArray>
#include <QLoggingCategory>
#include <QString>
#include <QtLogging>
#include <algorithm>
#include <atomic>
#include <charconv>

namespace heart_boom_maths::game::profiling
{

namespace
{

constexpr std::string_view SG_STATS_FLAG = "--profile-sg-stats";

// Qt's message handler is one function pointer for the process, so what it needs is file-local state.
struct LogState
{
    QtMessageHandler previous = nullptr;
    std::atomic<bool> on{false};
    std::atomic<int> opaque_nodes{0};
    std::atomic<int> opaque_batches{0};
    std::atomic<int> alpha_nodes{0};
    std::atomic<int> alpha_batches{0};
    std::atomic<int> max_nodes{0};
    std::atomic<int> max_batches{0};
};

LogState& state()
{
    static LogState log;
    return log;
}

// The number that follows `key` in `text`, or fallback.
int number_after(std::string_view text, std::string_view key, int fallback = 0) noexcept
{
    const std::size_t at = text.find(key);
    if (at == std::string_view::npos)
    {
        return fallback;
    }
    const std::string_view rest = text.substr(at + key.size());
    int value = fallback;
    const auto result = std::from_chars(rest.data(), rest.data() + rest.size(), value);
    return result.ec == std::errc{} ? value : fallback;
}

void record(const SceneGraphLine& line)
{
    LogState& log = state();
    switch (line.kind)
    {
    case SceneGraphLine::Kind::PASSES:
    {
        log.opaque_nodes.store(line.a, std::memory_order_relaxed);
        log.opaque_batches.store(line.b, std::memory_order_relaxed);
        log.alpha_nodes.store(line.c, std::memory_order_relaxed);
        log.alpha_batches.store(line.d, std::memory_order_relaxed);
        const int nodes = line.a + line.c;
        const int batches = line.b + line.d;
        int seen = log.max_nodes.load(std::memory_order_relaxed);
        while (nodes > seen && !log.max_nodes.compare_exchange_weak(seen, nodes, std::memory_order_relaxed))
        {
        }
        seen = log.max_batches.load(std::memory_order_relaxed);
        while (batches > seen && !log.max_batches.compare_exchange_weak(seen, batches, std::memory_order_relaxed))
        {
        }
        HBM_PLOT("qsg.nodes", nodes);
        HBM_PLOT("qsg.batches", batches);
        break;
    }
    case SceneGraphLine::Kind::RENDERER:
        HBM_PLOT("qsg.renderer_ms", line.a);
        HBM_PLOT("qsg.renderer_preprocess_ms", line.b);
        HBM_PLOT("qsg.renderer_updates_ms", line.c);
        HBM_PLOT("qsg.renderer_rendering_ms", line.d);
        break;
    case SceneGraphLine::Kind::FRAME:
        HBM_PLOT("qsg.frame_ms", line.a);
        HBM_PLOT("qsg.sync_ms", line.b);
        HBM_PLOT("qsg.render_ms", line.c);
        HBM_PLOT("qsg.swap_ms", line.d);
        break;
    case SceneGraphLine::Kind::NONE:
        break;
    }
}

void forward(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    const std::string_view category =
        context.category != nullptr ? std::string_view{context.category} : std::string_view{};
    const bool is_scene_graph = category.starts_with("qt.scenegraph") || category.starts_with("qt.rhi");
    // The renderer's own dump (QSG_RENDERER_DEBUG) is plain qDebug output; nothing else of this game is.
    const bool is_renderer_dump = type == QtDebugMsg && category == "default";
    if (!is_scene_graph && !is_renderer_dump)
    {
        if (state().previous != nullptr)
        {
            state().previous(type, context, message);
        }
        return;
    }
    const QByteArray text = message.toUtf8();
    record(parse_scene_graph_line(std::string_view{text.constData(), static_cast<std::size_t>(text.size())}));
}

} // namespace

SceneGraphLine parse_scene_graph_line(std::string_view text) noexcept
{
    SceneGraphLine line;
    if (const std::size_t opaque = text.find("-> Opaque:"); opaque != std::string_view::npos)
    {
        // Qt prints the frame's two passes in one message, the alpha pass after the opaque one.
        line.kind = SceneGraphLine::Kind::PASSES;
        const std::string_view from_opaque = text.substr(opaque);
        line.a = number_after(from_opaque, "Opaque: ");
        line.b = number_after(from_opaque, " nodes in ");
        if (const std::size_t alpha = from_opaque.find("-> Alpha:"); alpha != std::string_view::npos)
        {
            const std::string_view from_alpha = from_opaque.substr(alpha);
            line.c = number_after(from_alpha, "Alpha: ");
            line.d = number_after(from_alpha, " nodes in ");
        }
    }
    else if (text.find("time in renderer:") != std::string_view::npos)
    {
        line.kind = SceneGraphLine::Kind::RENDERER;
        line.a = number_after(text, "total=");
        line.b = number_after(text, "preprocess=");
        line.c = number_after(text, "updates=");
        line.d = number_after(text, "rendering=");
    }
    else if (text.find("syncAndRender: frame rendered in") != std::string_view::npos)
    {
        line.kind = SceneGraphLine::Kind::FRAME;
        line.a = number_after(text, "frame rendered in ");
        line.b = number_after(text, "sync=");
        line.c = number_after(text, "render=");
        line.d = number_after(text, "swap=");
    }
    return line;
}

void prepare_scene_graph_log(std::span<const std::string_view> arguments)
{
    // A scripted run wants the GPU time of its frames even when the profiler is not running.
    if (!profile::active() && std::ranges::find(arguments, RUN_FLAG) == arguments.end())
    {
        return;
    }
    // GPU timestamps are what "GPU frame time (ms)" is made of; without this the RHI does not ask for them.
    if (!qEnvironmentVariableIsSet("QSG_RHI_PROFILE"))
    {
        qputenv("QSG_RHI_PROFILE", "1");
    }
    if (std::ranges::find(arguments, SG_STATS_FLAG) == arguments.end() || state().on.exchange(true))
    {
        return;
    }
    qputenv("QSG_RENDERER_DEBUG", "render");
    QLoggingCategory::setFilterRules(QStringLiteral("qt.scenegraph.time.*=true"));
    state().previous = qInstallMessageHandler(forward);
}

SceneGraphCounts take_scene_graph_counts()
{
    LogState& log = state();
    SceneGraphCounts counts;
    counts.known = log.on.load();
    counts.opaque_nodes = log.opaque_nodes.load(std::memory_order_relaxed);
    counts.opaque_batches = log.opaque_batches.load(std::memory_order_relaxed);
    counts.alpha_nodes = log.alpha_nodes.load(std::memory_order_relaxed);
    counts.alpha_batches = log.alpha_batches.load(std::memory_order_relaxed);
    counts.max_nodes = log.max_nodes.exchange(0, std::memory_order_relaxed);
    counts.max_batches = log.max_batches.exchange(0, std::memory_order_relaxed);
    return counts;
}

} // namespace heart_boom_maths::game::profiling
