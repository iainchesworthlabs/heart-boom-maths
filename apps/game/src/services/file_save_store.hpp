// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "services/app_save_store.hpp"

#include <QString>
#include <optional>
#include <string>
#include <string_view>

namespace heart_boom_maths::game
{

/// The save file of iOS, Android and the desktop: one JSON text in `save.json` in a folder the app owns. A write goes
/// to a temporary file that is renamed over the old one, so a crash or a full disk leaves the old text in place.
class FileSaveStore final : public AppSaveStore
{
public:
    /// `directory` is created when the first write needs it.
    explicit FileSaveStore(QString directory);

    /// The folder for the real app: the per-user application data location.
    [[nodiscard]] static QString default_directory();

    std::optional<std::string> load() override;
    bool store(std::string_view text) override;
    bool set_damaged_aside() override;

    [[nodiscard]] QString file_path() const;
    [[nodiscard]] QString damaged_file_path() const;

private:
    QString m_Directory;
};

} // namespace heart_boom_maths::game
