// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "services/key_value_backend.hpp"

#include <QSettings>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace heart_boom_maths::game
{

/// A KeyValueBackend over QSettings. In a browser the native format of QSettings is the page's localStorage (Qt 6.12,
/// src/corelib/io/qsettings_wasm.cpp), which holds about 5 MB per origin. Everywhere else it is the platform's settings
/// store, which is how a desktop test exercises the same code.
class QSettingsBackend final : public KeyValueBackend
{
public:
    /// Takes the settings object, so a test can point it at a file of its own.
    explicit QSettingsBackend(std::unique_ptr<QSettings> settings);

    /// The native settings of this user and application. Qt's browser store keeps nothing when the organisation is
    /// empty, so both names are given here rather than read from the application object.
    [[nodiscard]] static std::unique_ptr<QSettingsBackend> native();

    [[nodiscard]] std::optional<std::string> get(std::string_view key) override;
    [[nodiscard]] bool set(std::string_view key, std::string_view value) override;
    [[nodiscard]] bool remove(std::string_view key) override;

private:
    std::unique_ptr<QSettings> m_Settings;
};

} // namespace heart_boom_maths::game
