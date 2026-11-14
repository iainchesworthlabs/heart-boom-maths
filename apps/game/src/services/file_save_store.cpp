// SPDX-License-Identifier: GPL-3.0-or-later
#include "services/file_save_store.hpp"

#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QStandardPaths>
#include <utility>

namespace heart_boom_maths::game
{

namespace
{

constexpr auto SAVE_FILE_NAME = "save.json";
constexpr auto DAMAGED_SUFFIX = ".bad";

} // namespace

FileSaveStore::FileSaveStore(QString directory) : m_Directory(std::move(directory)) {}

QString FileSaveStore::default_directory()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

QString FileSaveStore::file_path() const
{
    return QDir(m_Directory).filePath(QString::fromLatin1(SAVE_FILE_NAME));
}

QString FileSaveStore::damaged_file_path() const
{
    return file_path() + QString::fromLatin1(DAMAGED_SUFFIX);
}

std::optional<std::string> FileSaveStore::load()
{
    QFile file(file_path());
    if (!file.exists())
    {
        return std::nullopt;
    }
    if (!file.open(QIODevice::ReadOnly))
    {
        // A file that is there but cannot be read must not look like a first run: the next write would replace it.
        // Empty text is not a save, so the core calls it damaged and the file is set aside.
        return std::string();
    }
    const QByteArray bytes = file.readAll();
    return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

bool FileSaveStore::store(std::string_view text)
{
    if (!QDir().mkpath(m_Directory))
    {
        return false;
    }
    // QSaveFile writes to a temporary file in the same folder and renames it over the target on commit(), and discards
    // it when anything fails, so the old text survives a failed write.
    QSaveFile file(file_path());
    if (!file.open(QIODevice::WriteOnly))
    {
        return false;
    }
    if (file.write(text.data(), static_cast<qint64>(text.size())) != static_cast<qint64>(text.size()))
    {
        file.cancelWriting();
        return false;
    }
    return file.commit();
}

bool FileSaveStore::set_damaged_aside()
{
    const QString damaged = damaged_file_path();
    // Only the latest damaged file is kept: an older .bad is replaced.
    QFile::remove(damaged);
    return QFile::rename(file_path(), damaged);
}

} // namespace heart_boom_maths::game
