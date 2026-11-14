// SPDX-License-Identifier: GPL-3.0-or-later
#include "profile_run.hpp"

#include "game_controller.hpp"
#include "profiling.hpp"
#include "render_profiler.hpp"
#include "scene_graph_log.hpp"

#include <QCoreApplication>
#include <QFile>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMetaObject>
#include <QPoint>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScreen>
#include <QSize>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace heart_boom_maths::game::profiling
{

namespace
{

constexpr std::string_view SUMMARY_FLAG = "--profile-summary";

// A tap takes a moment more than the game's own tile guard (an app-layer comfort that ignores a tap right after a level
// starts), so a scripted answer is never thrown away by it.
constexpr std::int64_t SLOW_ANSWER_MS = 700;
constexpr std::int64_t FAST_ANSWER_MS = 140;
constexpr std::int64_t WRONG_ANSWER_MS = 450;
constexpr std::int64_t CARD_DWELL_MS = 500;
constexpr int STEP_INTERVAL_MS = 20;
constexpr char PROFILE_NAME[] = "PROFILE";

enum class Script
{
    TITLE,
    PLAY,
    ALERT,
    ENDLESS,
    TOP10,
};

} // namespace

struct ProfileRun::Phase
{
    const char* name;
    Script script;
    QSize window;
    int weight;
    std::int64_t duration_ms = 0; // filled in from the weights
};

bool parse_run_options(std::span<const std::string_view> arguments, RunOptions& options)
{
    bool found = false;
    for (std::size_t i = 0; i < arguments.size(); ++i)
    {
        if (arguments[i] == RUN_FLAG)
        {
            found = true;
            if (i + 1 < arguments.size())
            {
                const int seconds =
                    QString::fromUtf8(arguments[i + 1].data(), static_cast<qsizetype>(arguments[i + 1].size())).toInt();
                if (seconds > 0)
                {
                    options.seconds = seconds;
                }
            }
        }
        else if (arguments[i] == SUMMARY_FLAG && i + 1 < arguments.size())
        {
            options.summary_path =
                QString::fromUtf8(arguments[i + 1].data(), static_cast<qsizetype>(arguments[i + 1].size()));
        }
    }
    return found;
}

double percentile(std::vector<double> values, double percent)
{
    if (values.empty())
    {
        return 0.0;
    }
    std::ranges::sort(values);
    const double rank = std::ceil(std::clamp(percent, 0.0, 100.0) / 100.0 * static_cast<double>(values.size()));
    const auto index = static_cast<std::size_t>(std::max(rank, 1.0)) - 1;
    return values[std::min(index, values.size() - 1)];
}

namespace
{

double mean_of(const std::vector<double>& values)
{
    if (values.empty())
    {
        return 0.0;
    }
    double sum = 0.0;
    for (const double value : values)
    {
        sum += value;
    }
    return sum / static_cast<double>(values.size());
}

} // namespace

FrameStatistics summarise_frames(std::span<const PhaseFrame> frames, double refresh_ms)
{
    FrameStatistics stats;
    std::vector<double> intervals;
    std::vector<double> work;
    std::vector<double> gpu;
    for (const PhaseFrame& frame : frames)
    {
        if (frame.interval_ms > IDLE_GAP_MS)
        {
            ++stats.idle_gaps;
            continue;
        }
        intervals.push_back(frame.interval_ms);
        work.push_back(frame.work_ms);
        if (frame.gpu_ms >= 0.0)
        {
            gpu.push_back(frame.gpu_ms);
        }
    }
    stats.frames = static_cast<int>(intervals.size());
    if (!intervals.empty())
    {
        stats.mean_ms = mean_of(intervals);
        stats.p95_ms = percentile(intervals, 95.0);
        stats.p99_ms = percentile(intervals, 99.0);
        stats.max_ms = *std::ranges::max_element(intervals);
        stats.dropped =
            static_cast<int>(std::ranges::count_if(intervals, [](double v) { return v > DROPPED_FRAME_MS; }));
        stats.work_mean_ms = mean_of(work);
        stats.work_p95_ms = percentile(work, 95.0);
        stats.work_p99_ms = percentile(work, 99.0);
        stats.work_max_ms = *std::ranges::max_element(work);
        stats.over_budget =
            static_cast<int>(std::ranges::count_if(work, [refresh_ms](double v) { return v > refresh_ms; }));
    }
    stats.gpu_frames = static_cast<int>(gpu.size());
    if (!gpu.empty())
    {
        stats.gpu_mean_ms = mean_of(gpu);
        stats.gpu_p95_ms = percentile(gpu, 95.0);
        stats.gpu_max_ms = *std::ranges::max_element(gpu);
    }
    return stats;
}

ProfileRun::ProfileRun(GameController& controller, QQuickWindow& window, RenderProfiler& render, RunOptions options,
                       QObject* parent)
    : QObject(parent), m_Controller(controller), m_Window(window), m_Render(render), m_Options(std::move(options))
{
    // The phone window is the one the program opens; the tablet and the desktop are the two larger shapes of the other
    // size class (Theme.compactMaxWidth is 600).
    const QSize phone(390, 844);
    const QSize tablet(820, 1180);
    const QSize desktop(1280, 800);
    m_Phases = {
        {"title-phone", Script::TITLE, phone, 4},    {"play-phone", Script::PLAY, phone, 12},
        {"alert-phone", Script::ALERT, phone, 6},    {"play-tablet", Script::PLAY, tablet, 12},
        {"play-desktop", Script::PLAY, desktop, 12}, {"endless-desktop", Script::ENDLESS, desktop, 40},
        {"top10-phone", Script::TOP10, phone, 3},    {"top10-tablet", Script::TOP10, tablet, 3},
    };
    int total_weight = 0;
    for (const Phase& phase : m_Phases)
    {
        total_weight += phase.weight;
    }
    for (Phase& phase : m_Phases)
    {
        phase.duration_ms = static_cast<std::int64_t>(m_Options.seconds) * 1000 * phase.weight / total_weight;
    }
    m_Timer.setInterval(STEP_INTERVAL_MS);
    m_Timer.setTimerType(Qt::PreciseTimer);
    connect(&m_Timer, &QTimer::timeout, this, &ProfileRun::step);
}

void ProfileRun::start()
{
    // The window stays where the system opened it; the summary names the screen and its refresh rate.
    const QScreen* screen = m_Window.screen();
    m_Summary = QJsonObject{{"run_seconds", m_Options.seconds},
                            {"refresh_ms", refresh_period_ms()},
                            {"screen", screen != nullptr ? screen->name() : QString()},
                            {"device_pixel_ratio", screen != nullptr ? screen->devicePixelRatio() : 1.0}};
    begin_phase(0);
    m_Timer.start();
}

int ProfileRun::visible_items() const
{
    int count = 0;
    std::vector<QQuickItem*> pending{m_Window.contentItem()};
    while (!pending.empty())
    {
        QQuickItem* item = pending.back();
        pending.pop_back();
        if (item == nullptr || !item->isVisible())
        {
            continue;
        }
        ++count;
        for (QQuickItem* child : item->childItems())
        {
            pending.push_back(child);
        }
    }
    return count;
}

void ProfileRun::begin_phase(std::size_t index)
{
    m_Current = index;
    const Phase& phase = m_Phases[index];
    HBM_MESSAGE_DYNAMIC(std::string_view(phase.name));
    HBM_PLOT("profile.phase", index);
    m_Window.resize(phase.window);
    m_NextActionMs = 0;
    m_Counter = 0;
    m_PhaseClock.start();
    // What the earlier phase presented is not this one's.
    (void)m_Render.take_frames();
    (void)take_scene_graph_counts();

    switch (phase.script)
    {
    case Script::TITLE:
    case Script::TOP10:
        leave_game_for_title();
        break;
    case Script::ALERT:
        // The alert comes from a level already running: the earlier phase left one going, and a new game covers the
        // case where it ended.
        if (m_Controller.state() != GameController::State::Playing)
        {
            m_Controller.startGame();
        }
        break;
    case Script::PLAY:
    case Script::ENDLESS:
        if (m_Controller.state() == GameController::State::Idle ||
            m_Controller.state() == GameController::State::GameOver)
        {
            m_Controller.startGame();
        }
        break;
    }
}

void ProfileRun::leave_game_for_title()
{
    // A game in progress ends the way a player ends it: pause, quit, then the name for the top 10.
    if (m_Controller.state() == GameController::State::Playing)
    {
        m_Controller.pause();
    }
    if (m_Controller.state() == GameController::State::Paused)
    {
        m_Controller.quit();
    }
    save_a_name();
}

void ProfileRun::save_a_name()
{
    if (m_Controller.name_entry_pending())
    {
        (void)m_Controller.saveName(QString::fromLatin1(PROFILE_NAME));
    }
}

void ProfileRun::show_top10()
{
    if (auto* shell = m_Window.findChild<QObject*>(QStringLiteral("appShell")); shell != nullptr)
    {
        QMetaObject::invokeMethod(shell, "showTop10");
    }
}

void ProfileRun::play_script(const Phase& phase, std::int64_t phase_ms)
{
    switch (m_Controller.state())
    {
    case GameController::State::LevelClear:
        if (phase_ms >= m_NextActionMs)
        {
            m_Controller.nextLevel();
            m_NextActionMs = phase_ms + CARD_DWELL_MS;
        }
        return;
    case GameController::State::Won:
        if (phase_ms >= m_NextActionMs)
        {
            m_Controller.keepGoing();
            m_NextActionMs = phase_ms + CARD_DWELL_MS;
        }
        return;
    case GameController::State::Paused:
        m_Controller.resume();
        return;
    case GameController::State::GameOver:
        save_a_name();
        if (phase_ms >= m_NextActionMs)
        {
            m_Controller.startGame();
            m_NextActionMs = phase_ms + CARD_DWELL_MS;
        }
        return;
    case GameController::State::Idle:
        m_Controller.startGame();
        return;
    case GameController::State::Playing:
        break;
    }
    if (phase_ms < m_NextActionMs)
    {
        return;
    }
    const SessionView& view = m_Controller.session_view();
    ++m_Counter;
    if (phase.script == Script::ALERT)
    {
        // Wrong answers take time off the bomb, so the alert comes in seconds. Once it is on, the child stops: the
        // alert's own look is what is measured.
        if (!m_Controller.alert())
        {
            const int wrong = (view.question.correct_tile + 1) % 4;
            m_Controller.answer(view.removed_tiles & (1 << wrong) ? (wrong + 1) % 4 : wrong);
        }
        m_NextActionMs = phase_ms + WRONG_ANSWER_MS;
        return;
    }
    const bool endless_play = phase.script == Script::ENDLESS;
    // A hint now and then, a wrong answer one time in eight: a child's play and not a machine's.
    if (!endless_play && m_Counter % 9 == 0 && m_Controller.hint_available())
    {
        m_Controller.useHint();
    }
    int tile = view.question.correct_tile;
    if (!endless_play && m_Counter % 8 == 0)
    {
        for (int candidate = 0; candidate < 4; ++candidate)
        {
            if (candidate != tile && (view.removed_tiles & (1 << candidate)) == 0)
            {
                tile = candidate;
                break;
            }
        }
    }
    m_Controller.answer(tile);
    m_NextActionMs = phase_ms + (endless_play ? FAST_ANSWER_MS : SLOW_ANSWER_MS);
}

void ProfileRun::step()
{
    if (m_Finished)
    {
        return;
    }
    const Phase& phase = m_Phases[m_Current];
    const std::int64_t phase_ms = m_PhaseClock.elapsed();
    switch (phase.script)
    {
    case Script::TITLE:
        break;
    case Script::TOP10:
        if (m_Counter == 0)
        {
            show_top10();
            m_Counter = 1;
        }
        break;
    case Script::PLAY:
    case Script::ALERT:
    case Script::ENDLESS:
        play_script(phase, phase_ms);
        break;
    }
    if (phase_ms >= phase.duration_ms)
    {
        end_phase();
        if (m_Current + 1 < m_Phases.size())
        {
            begin_phase(m_Current + 1);
        }
        else
        {
            finish();
        }
    }
}

double ProfileRun::refresh_period_ms() const
{
    const QScreen* screen = m_Window.screen();
    const double hz = screen != nullptr ? screen->refreshRate() : 60.0;
    return 1000.0 / (hz > 1.0 ? hz : 60.0);
}

void ProfileRun::end_phase()
{
    const Phase& phase = m_Phases[m_Current];
    std::vector<PhaseFrame> frames;
    for (const FrameSample& sample : m_Render.take_frames())
    {
        frames.push_back({sample.interval_ms, sample.work_ms, sample.gpu_ms});
    }
    const FrameStatistics stats = summarise_frames(frames, refresh_period_ms());
    QJsonObject entry{
        {"window", QStringLiteral("%1x%2").arg(phase.window.width()).arg(phase.window.height())},
        {"seconds", static_cast<double>(m_PhaseClock.elapsed()) / 1000.0},
        {"level", m_Controller.level()},
        {"visible_items", visible_items()},
        {"frames", stats.frames},
        {"idle_gaps", stats.idle_gaps},
        {"frame_mean_ms", stats.mean_ms},
        {"frame_p95_ms", stats.p95_ms},
        {"frame_p99_ms", stats.p99_ms},
        {"frame_max_ms", stats.max_ms},
        {"dropped", stats.dropped},
        {"work_mean_ms", stats.work_mean_ms},
        {"work_p95_ms", stats.work_p95_ms},
        {"work_p99_ms", stats.work_p99_ms},
        {"work_max_ms", stats.work_max_ms},
        {"over_budget", stats.over_budget},
        {"gpu_frames", stats.gpu_frames},
        {"gpu_mean_ms", stats.gpu_mean_ms},
        {"gpu_p95_ms", stats.gpu_p95_ms},
        {"gpu_max_ms", stats.gpu_max_ms},
    };
    // Only when the run was asked for the scene graph's own numbers (--profile-sg-stats): reading them costs frame
    // time.
    if (const SceneGraphCounts sg = take_scene_graph_counts(); sg.known)
    {
        entry.insert("sg_opaque_nodes", sg.opaque_nodes);
        entry.insert("sg_opaque_batches", sg.opaque_batches);
        entry.insert("sg_alpha_nodes", sg.alpha_nodes);
        entry.insert("sg_alpha_batches", sg.alpha_batches);
        entry.insert("sg_max_nodes", sg.max_nodes);
        entry.insert("sg_max_batches", sg.max_batches);
    }
    QJsonArray phases = m_Summary.value("phases").toArray();
    QJsonObject named{{"name", QString::fromLatin1(phase.name)}};
    for (auto it = entry.begin(); it != entry.end(); ++it)
    {
        named.insert(it.key(), it.value());
    }
    phases.append(named);
    m_Summary.insert("phases", phases);
}

void ProfileRun::finish()
{
    m_Finished = true;
    m_Timer.stop();
    m_Summary.insert("first_frame_ms", m_Render.first_frame_ms());
    HBM_MESSAGE("profile.finished");
    const QByteArray json = QJsonDocument(m_Summary).toJson(QJsonDocument::Indented);
    if (!m_Options.summary_path.isEmpty())
    {
        QFile file(m_Options.summary_path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            file.write(json);
        }
        else
        {
            std::fprintf(stderr, "Cannot write %s\n", qPrintable(m_Options.summary_path));
        }
    }
    else
    {
        std::fwrite(json.constData(), 1, static_cast<std::size_t>(json.size()), stdout);
    }
    QCoreApplication::quit();
}

bool start_profile_run(GameController& controller, QQuickWindow& window, std::span<const std::string_view> arguments)
{
    RunOptions options;
    if (!parse_run_options(arguments, options))
    {
        return false;
    }
    auto* render = window.findChild<RenderProfiler*>();
    if (render == nullptr)
    {
        return false;
    }
    // Owned by the window.
    auto* run =
        new ProfileRun(controller, window, *render, options, &window); // NOLINT(cppcoreguidelines-owning-memory)
    run->start();
    return true;
}

} // namespace heart_boom_maths::game::profiling
