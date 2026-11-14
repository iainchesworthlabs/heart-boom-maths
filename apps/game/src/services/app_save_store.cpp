// SPDX-License-Identifier: GPL-3.0-or-later
#include "services/app_save_store.hpp"

#include "services/file_save_store.hpp"
#include "services/qsettings_backend.hpp"
#include "services/web_save_store.hpp"

namespace heart_boom_maths::game
{

std::unique_ptr<AppSaveStore>
make_save_store(platform::SaveStorage storage, const QString& directory,
                const std::function<std::unique_ptr<KeyValueBackend>()>& make_browser_backend)
{
    switch (storage)
    {
    case platform::SaveStorage::BROWSER:
        return std::make_unique<WebSaveStore>(make_browser_backend());
    case platform::SaveStorage::SESSION:
        return std::make_unique<WebSaveStore>(std::make_unique<MemoryKeyValueBackend>());
    case platform::SaveStorage::LOCAL_FILE:
        break;
    }
    return std::make_unique<FileSaveStore>(directory);
}

std::unique_ptr<AppSaveStore> make_save_store(platform::SaveStorage storage, const QString& directory)
{
    return make_save_store(storage, directory, [] { return QSettingsBackend::native(); });
}

} // namespace heart_boom_maths::game
