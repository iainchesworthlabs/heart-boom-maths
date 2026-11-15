// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace heart_boom_maths::game
{

/// The family's own cloud copy of the shared save part (the players and the top 10), as one piece of text. On an Apple
/// device it is one key of the iCloud key-value store of the Apple Account signed in on that device; the game has no
/// account, server or network code of its own, and nothing here reaches the developer. Every other platform has
/// NullCloudStore.
///
/// A store does nothing until the game asks: constructing one, and available(), touch no cloud API that reads or writes
/// data, so a default install (the Settings switch off) never contacts the cloud. read(), write() and a change handler
/// are used only while the switch is on. Every call comes from the application's main thread, and none blocks.
class CloudStore
{
public:
    /// Called on the main thread when the store reports a change that another device made (or an account change). It
    /// carries nothing: the game reads the store itself.
    using ChangeHandler = std::function<void()>;

    virtual ~CloudStore() = default;

    /// True when this platform can sync and the person is signed in to the account that does it. The Settings row is
    /// shown only then. Must be cheap and must not read or write the cloud copy.
    [[nodiscard]] virtual bool available() const = 0;

    /// The payload text, or nothing when there is none (or the value is not text).
    [[nodiscard]] virtual std::optional<std::string> read() = 0;

    /// Replaces the payload text. Returns false when the store did not take it; the caller logs it and tries again at
    /// its next sync.
    virtual bool write(std::string_view text) = 0;

    /// Registers the handler; an empty one removes it. The first non-empty handler is what starts the store listening
    /// for changes from other devices, and an empty one stops it. A call that was already queued may still arrive after
    /// the handler was removed, so the caller must tolerate that.
    virtual void set_change_handler(ChangeHandler handler) = 0;
};

/// The store of every platform without iCloud, and the default of a controller that is handed none. Never available.
class NullCloudStore final : public CloudStore
{
public:
    [[nodiscard]] bool available() const override
    {
        return false;
    }

    [[nodiscard]] std::optional<std::string> read() override
    {
        return std::nullopt;
    }

    bool write(std::string_view) override
    {
        return false;
    }

    void set_change_handler(ChangeHandler) override {}
};

} // namespace heart_boom_maths::game
