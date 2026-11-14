// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QElapsedTimer>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QTimer>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

class QQuickWindow;

namespace heart_boom_maths::game
{
class GameController;
}

namespace heart_boom_maths::game::profiling
{

class RenderProfiler;

/// What a run of the game is told on the command line.
struct RunOptions
{
    int seconds = 90;     // the whole run, split between the phases in proportion to their weights
    QString summary_path; // where the per-phase statistics are written as JSON; empty for none
};

/// Reads --profile-run <seconds> and --profile-summary <file>. Returns false when --profile-run is not there.
[[nodiscard]] bool parse_run_options(std::span<const std::string_view> arguments, RunOptions& options);

/// A gap between two presentations longer than this is a screen that was not animating (a Qt Quick window draws a frame
/// only when something changed), not a frame that was late; it is counted apart and kept out of the frame times.
inline constexpr double IDLE_GAP_MS = 250.0;

/// A gap between two presentations longer than this is a frame missed at 60 frames a second (two periods of it). The
/// game animates at that rate whatever the display's refresh rate, and a window that has nothing to move draws nothing,
/// so a long gap is either a late frame or a pause; the render thread's work per frame says which.
inline constexpr double DROPPED_FRAME_MS = 2000.0 / 60.0;

/// Frame-time statistics of one phase, from the frames the scene graph presented in it. Pure arithmetic, tested apart
/// from the window.
struct FrameStatistics
{
    int frames = 0; // presented frames, not counting idle gaps
    int idle_gaps = 0;
    double mean_ms = 0.0; // time between presentations
    double p95_ms = 0.0;
    double p99_ms = 0.0;
    double max_ms = 0.0;
    int dropped =
        0; // intervals longer than DROPPED_FRAME_MS: a 60 fps frame missed, or a pause while the screen held still
    double work_mean_ms = 0.0; // the render thread's time recording the frame (beforeRendering to afterRendering), not
                               // the wait for the display
    double work_p95_ms = 0.0;
    double work_p99_ms = 0.0;
    double work_max_ms = 0.0;
    int over_budget =
        0; // frames whose work took longer than one refresh period: a frame that could not have been ready in time
    int gpu_frames = 0;
    double gpu_mean_ms = 0.0;
    double gpu_p95_ms = 0.0;
    double gpu_max_ms = 0.0;
};

struct PhaseFrame
{
    double interval_ms = 0.0;
    double work_ms = 0.0;
    double gpu_ms = -1.0;
};

/// The statistics of `frames` against a display that refreshes every `refresh_ms`. A frame with a negative gpu_ms has
/// no GPU time and is left out of the GPU figures.
[[nodiscard]] FrameStatistics summarise_frames(std::span<const PhaseFrame> frames, double refresh_ms);

/// The nearest-rank percentile (0 to 100) of `values`, which need not be sorted. 0 for no values.
[[nodiscard]] double percentile(std::vector<double> values, double percent);

/// Plays scripted games through the real controller and the real window for a fixed time, so that a trace shows what
/// the game does when it is played: the title, a level on a phone, a tablet and a desktop window, the alert, endless
/// play and the top 10. Each phase is named in the trace by a message and a plot, and its frame statistics go in the
/// summary. It quits the program when it is done. A profiling build only: no other build contains it.
class ProfileRun : public QObject
{
    Q_OBJECT

public:
    ProfileRun(GameController& controller, QQuickWindow& window, RenderProfiler& render, RunOptions options,
               QObject* parent = nullptr);

    void start();

private:
    struct Phase;
    void step();
    void begin_phase(std::size_t index);
    void end_phase();
    void finish();
    void play_script(const Phase& phase, std::int64_t phase_ms);
    void leave_game_for_title();
    void save_a_name();
    void show_top10();
    [[nodiscard]] int visible_items() const;
    [[nodiscard]] double refresh_period_ms() const;

    GameController& m_Controller;
    QQuickWindow& m_Window;
    RenderProfiler& m_Render;
    RunOptions m_Options;
    QTimer m_Timer;
    QElapsedTimer m_PhaseClock;
    std::vector<Phase> m_Phases;
    std::size_t m_Current = 0;
    std::int64_t m_NextActionMs = 0; // in phase time
    int m_Counter = 0;
    bool m_Finished = false;
    QJsonObject m_Summary;
};

} // namespace heart_boom_maths::game::profiling
