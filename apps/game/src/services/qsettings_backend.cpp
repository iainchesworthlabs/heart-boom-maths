// SPDX-License-Identifier: GPL-3.0-or-later
#include "services/qsettings_backend.hpp"

#include <QString>
#include <QVariant>
#include <utility>

namespace heart_boom_maths::game
{

namespace
{

// The organisation is the namespace the browser prefixes its localStorage keys with, so it must be stable. It is the
// owner of the repository, as in the application identifier, and names no person.
constexpr auto ORGANISATION = "iainchesworthlabs";
constexpr auto APPLICATION = "heart-boom-maths";

QString to_qstring(std::string_view text)
{
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

} // namespace

QSettingsBackend::QSettingsBackend(std::unique_ptr<QSettings> settings) : m_Settings(std::move(settings)) {}

std::unique_ptr<QSettingsBackend> QSettingsBackend::native()
{
    return std::make_unique<QSettingsBackend>(std::make_unique<QSettings>(QSettings::NativeFormat, QSettings::UserScope,
                                                                          QString::fromLatin1(ORGANISATION),
                                                                          QString::fromLatin1(APPLICATION)));
}

std::optional<std::string> QSettingsBackend::get(std::string_view key)
{
    const QString name = to_qstring(key);
    if (!m_Settings->contains(name))
    {
        return std::nullopt;
    }
    return m_Settings->value(name).toString().toStdString();
}

bool QSettingsBackend::set(std::string_view key, std::string_view value)
{
    m_Settings->setValue(to_qstring(key), to_qstring(value));
    m_Settings->sync();
    return m_Settings->status() == QSettings::NoError;
}

bool QSettingsBackend::remove(std::string_view key)
{
    m_Settings->remove(to_qstring(key));
    m_Settings->sync();
    return m_Settings->status() == QSettings::NoError;
}

} // namespace heart_boom_maths::game
