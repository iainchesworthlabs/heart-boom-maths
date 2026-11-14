// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// What the performance tests share, for the rules library and for the game. A test times some work several times, takes
// the median and the best, and hands them to check(), which compares the median with the budget in
// tools/perf-budgets.json, prints one result line, writes a result file for tracking and fails the test only when the
// median is over the budget. See "Performance tests" in docs/testing.md.
//
// The harness is plain C++ over the standard library and the core's own JSON reader (which reads the budget file), so
// the game's performance tests include it from here and no second copy exists.

#include "heart_boom_maths/json.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <utility>
#include <vector>

// HBM_PERF_BUDGETS_FILE, defined by the CMake of the test program, is the path of tools/perf-budgets.json.

namespace hbm_perf
{

using SteadyClock = std::chrono::steady_clock;

/// Times the part of a repetition that is the work, so setup before it is not counted. A repetition that never calls
/// start() is timed from start to finish.
class Timer
{
public:
    void start()
    {
        m_Started = true;
        m_Begin = SteadyClock::now();
    }

    void stop()
    {
        m_Elapsed += SteadyClock::now() - m_Begin;
    }

    [[nodiscard]] double elapsed_ns() const
    {
        return std::chrono::duration<double, std::nano>(m_Elapsed).count();
    }

    [[nodiscard]] bool started() const
    {
        return m_Started;
    }

private:
    bool m_Started = false;
    SteadyClock::time_point m_Begin{};
    SteadyClock::duration m_Elapsed{};
};

struct Stats
{
    double median = 0;
    double best = 0; // the smallest value: the run least disturbed by the machine
    double mean = 0;
    double p95 = 0;
    double p99 = 0;
    double max = 0;
    std::size_t count = 0;
};

/// The value at a share (0 to 100) of a sorted list, by nearest rank.
[[nodiscard]] inline double percentile_of_sorted(const std::vector<double>& sorted, double percent)
{
    if (sorted.empty())
    {
        return 0;
    }
    const double rank = percent / 100.0 * static_cast<double>(sorted.size());
    const auto index = static_cast<std::size_t>(std::max(1.0, std::ceil(rank))) - 1;
    return sorted[std::min(index, sorted.size() - 1)];
}

[[nodiscard]] inline Stats stats_of(std::vector<double> values)
{
    Stats s;
    if (values.empty())
    {
        return s;
    }
    std::sort(values.begin(), values.end());
    const std::size_t n = values.size();
    s.count = n;
    s.best = values.front();
    s.max = values.back();
    s.median = n % 2 == 1 ? values[n / 2] : (values[n / 2 - 1] + values[n / 2]) / 2.0;
    double sum = 0;
    for (double v : values)
    {
        sum += v;
    }
    s.mean = sum / static_cast<double>(n);
    s.p95 = percentile_of_sorted(values, 95);
    s.p99 = percentile_of_sorted(values, 99);
    return s;
}

/// A number that is not a time (allocations, bytes, objects) as a measurement: one exact value, no spread.
[[nodiscard]] inline Stats exact(double value)
{
    Stats s;
    s.median = s.best = s.mean = s.p95 = s.p99 = s.max = value;
    s.count = 1;
    return s;
}

/// Allocations per operation in thousandths, so that the budget file holds only integers.
[[nodiscard]] inline double milli_allocations(std::int64_t allocations, std::int64_t operations)
{
    return 1000.0 * static_cast<double>(allocations) / static_cast<double>(operations);
}

/// Runs `work(timer)` once to warm up and then `repetitions` times, and returns the nanoseconds per operation of each
/// repetition. `work` returns how many operations it did. It may call timer.start() and timer.stop() around the part
/// that counts; without that the whole call is timed.
template <typename Work>
[[nodiscard]] std::vector<double> sample_ns_per_op(int repetitions, Work&& work)
{
    std::vector<double> values;
    for (int i = -1; i < repetitions; ++i)
    {
        Timer timer;
        const auto begin = SteadyClock::now();
        const double operations = static_cast<double>(work(timer));
        const auto end = SteadyClock::now();
        const double ns =
            timer.started() ? timer.elapsed_ns() : std::chrono::duration<double, std::nano>(end - begin).count();
        if (i >= 0) // the first call only warms the caches and the allocator
        {
            values.push_back(ns / operations);
        }
    }
    return values;
}

/// A number that goes in the result file and the result line but has no budget of its own.
struct Extra
{
    std::string name;
    double value;
};

namespace detail
{

struct Budget
{
    std::string metric;
    std::string unit;
    std::int64_t value = 0;
};

[[nodiscard]] inline std::string env_or(const char* name, const std::string& fallback)
{
    const char* value = std::getenv(name);
    return value != nullptr && *value != '\0' ? std::string(value) : fallback;
}

/// The budgets, read once. A file that cannot be read gives an empty table and a message the first test prints.
struct BudgetTable
{
    std::vector<std::pair<std::string, Budget>> entries;
    std::string problem;

    BudgetTable()
    {
        std::ifstream in(HBM_PERF_BUDGETS_FILE, std::ios::binary);
        if (!in)
        {
            problem = std::string("cannot open ") + HBM_PERF_BUDGETS_FILE;
            return;
        }
        in.seekg(0, std::ios::end);
        std::string text(static_cast<std::size_t>(in.tellg()), ' ');
        in.seekg(0);
        in.read(text.data(), static_cast<std::streamsize>(text.size()));
        const auto parsed = heart_boom_maths::json::parse(text);
        if (!parsed)
        {
            problem = std::string("cannot parse ") + HBM_PERF_BUDGETS_FILE;
            return;
        }
        const auto* list = parsed->find("budgets") != nullptr ? parsed->find("budgets")->get_array() : nullptr;
        if (list == nullptr)
        {
            problem = std::string(HBM_PERF_BUDGETS_FILE) + " has no \"budgets\" array";
            return;
        }
        for (const auto& item : *list)
        {
            const auto* name = item.find("name") != nullptr ? item.find("name")->get_string() : nullptr;
            const auto* metric = item.find("metric") != nullptr ? item.find("metric")->get_string() : nullptr;
            const auto* unit = item.find("unit") != nullptr ? item.find("unit")->get_string() : nullptr;
            const auto* value = item.find("budget") != nullptr ? item.find("budget")->get_integer() : nullptr;
            if (name == nullptr || metric == nullptr || unit == nullptr || value == nullptr)
            {
                problem = "a budget entry lacks name, metric, unit or budget";
                return;
            }
            entries.push_back({*name, Budget{*metric, *unit, *value}});
        }
    }

    [[nodiscard]] const Budget* find(const std::string& name) const
    {
        for (const auto& entry : entries)
        {
            if (entry.first == name)
            {
                return &entry.second;
            }
        }
        return nullptr;
    }
};

[[nodiscard]] inline const BudgetTable& budgets()
{
    static const BudgetTable table;
    return table;
}

/// "%.6g" without the locale: a result file is read by a script.
[[nodiscard]] inline std::string number(double value)
{
    char text[64];
    std::snprintf(text, sizeof text, "%.6g", value);
    return text;
}

inline void write_result(const std::string& name, const std::string& json)
{
    namespace fs = std::filesystem;
    const fs::path dir = env_or("HBM_PERF_RESULTS_DIR", "perf-results");
    std::error_code ignored;
    fs::create_directories(dir, ignored);
    std::ofstream out(dir / (name + ".json"), std::ios::binary | std::ios::trunc);
    out << json << '\n';
}

} // namespace detail

/// Reports one measurement against its budget. The test fails when the median is over the budget, or when there is no
/// budget for `name` (add it with tools/ci/perf_report.py --update-budgets). Setting HBM_PERF_NO_BUDGET=1 makes it
/// report only, which is how a local run for new budgets is made.
inline void check(const std::string& name, const std::string& metric, const std::string& unit, const Stats& stats,
                  const std::vector<Extra>& extras = {})
{
    const bool enforce = detail::env_or("HBM_PERF_NO_BUDGET", "") != "1";
    const detail::Budget* budget = detail::budgets().find(name);
    bool pass = true;
    std::string why;
    if (!detail::budgets().problem.empty())
    {
        pass = !enforce;
        why = detail::budgets().problem;
    }
    else if (budget == nullptr)
    {
        pass = !enforce;
        why = "no budget in tools/perf-budgets.json (run tools/ci/perf_report.py --update-budgets)";
    }
    else if (budget->unit != unit || budget->metric != metric)
    {
        pass = !enforce;
        why = "the budget is for " + budget->metric + " in " + budget->unit + ", this test reports " + metric + " in " +
              unit;
    }
    else
    {
        pass = !enforce || stats.median <= static_cast<double>(budget->value);
        if (!pass)
        {
            why = "median " + detail::number(stats.median) + " " + unit + " is over the budget of " +
                  std::to_string(budget->value);
        }
    }

    const std::string budget_text = budget != nullptr ? std::to_string(budget->value) : "none";
    std::string extra_text;
    for (const Extra& extra : extras)
    {
        extra_text += " " + extra.name + "=" + detail::number(extra.value);
    }
    std::printf("PERF name=%s metric=%s unit=%s median=%s best=%s budget=%s pass=%s%s\n", name.c_str(), metric.c_str(),
                unit.c_str(), detail::number(stats.median).c_str(), detail::number(stats.best).c_str(),
                budget_text.c_str(), pass ? "true" : "false", extra_text.c_str());
    std::fflush(stdout);

    std::string json = "{\"name\":\"" + name + "\",\"metric\":\"" + metric + "\",\"unit\":\"" + unit +
                       "\",\"median\":" + detail::number(stats.median) + ",\"best\":" + detail::number(stats.best) +
                       ",\"mean\":" + detail::number(stats.mean) + ",\"p95\":" + detail::number(stats.p95) +
                       ",\"p99\":" + detail::number(stats.p99) + ",\"max\":" + detail::number(stats.max) +
                       ",\"repetitions\":" + std::to_string(stats.count) +
                       ",\"budget\":" + (budget != nullptr ? std::to_string(budget->value) : "null") +
                       ",\"budgeted\":true,\"pass\":" + (pass ? "true" : "false");
    for (const Extra& extra : extras)
    {
        json += ",\"" + extra.name + "\":" + detail::number(extra.value);
    }
    json += "}";
    detail::write_result(name, json);

    INFO(name << ": " << why);
    CHECK(pass);
}

/// A number that is only reported (a frame-time p99, a count of slow frames). It goes in the result file and the result
/// line with `budgeted` false and never fails a test.
inline void note(const std::string& name, const std::string& metric, const std::string& unit, double value)
{
    std::printf("PERF name=%s metric=%s unit=%s median=%s best=%s budget=none pass=true\n", name.c_str(),
                metric.c_str(), unit.c_str(), detail::number(value).c_str(), detail::number(value).c_str());
    std::fflush(stdout);
    detail::write_result(name, "{\"name\":\"" + name + "\",\"metric\":\"" + metric + "\",\"unit\":\"" + unit +
                                   "\",\"median\":" + detail::number(value) + ",\"best\":" + detail::number(value) +
                                   ",\"budget\":null,\"budgeted\":false,\"pass\":true}");
}

} // namespace hbm_perf
