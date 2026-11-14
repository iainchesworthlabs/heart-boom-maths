// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "services/app_save_store.hpp"
#include "services/key_value_backend.hpp"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace heart_boom_maths::game
{

/// The save of the browser build: the whole save text under one key of a KeyValueBackend. The save is a few kilobytes,
/// so one value is far below the 5 MB a browser allows an origin, and there is nothing to merge or migrate.
///
/// A browser has no file to rename, so the "atomic write" is the backend's single set of one key: a failed set leaves
/// the old text. A save the core cannot read is copied to a second key before the next write would replace it, which is
/// the browser's save.json.bad.
class WebSaveStore final : public AppSaveStore
{
public:
    explicit WebSaveStore(std::unique_ptr<KeyValueBackend> backend);

    /// Where the save and the damaged copy are kept; the names of the files of the other platforms.
    static constexpr std::string_view SAVE_KEY = "save.json";
    static constexpr std::string_view DAMAGED_KEY = "save.json.bad";

    std::optional<std::string> load() override;
    bool store(std::string_view text) override;
    bool set_damaged_aside() override;

private:
    std::unique_ptr<KeyValueBackend> m_Backend;
};

} // namespace heart_boom_maths::game
