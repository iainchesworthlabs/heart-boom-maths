// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace heart_boom_maths::game
{

/// A small place that keeps text under names. The browser build keeps the save in one (WebSaveStore), and the interface
/// is what lets a desktop test prove how that store behaves when the browser's storage is blocked or full.
class KeyValueBackend
{
public:
    virtual ~KeyValueBackend() = default;

    /// The text kept under `key`, or nothing when there is none.
    [[nodiscard]] virtual std::optional<std::string> get(std::string_view key) = 0;

    /// Replaces the text under `key`. Returns false when it could not be kept, and then the old text is still there.
    [[nodiscard]] virtual bool set(std::string_view key, std::string_view value) = 0;

    /// Forgets `key`. Returns false when that failed; a key that was not there counts as removed.
    [[nodiscard]] virtual bool remove(std::string_view key) = 0;
};

/// Keeps the text for the life of the process only. It is what the web build falls back to when the browser lets the
/// page keep nothing (a blocked or private window): the game runs and saves for the session and starts fresh at the
/// next load.
class MemoryKeyValueBackend final : public KeyValueBackend
{
public:
    [[nodiscard]] std::optional<std::string> get(std::string_view key) override;
    [[nodiscard]] bool set(std::string_view key, std::string_view value) override;
    [[nodiscard]] bool remove(std::string_view key) override;

private:
    std::map<std::string, std::string, std::less<>> m_Values;
};

} // namespace heart_boom_maths::game
