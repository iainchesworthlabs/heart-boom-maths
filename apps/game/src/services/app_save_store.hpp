// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "heart_boom_maths/save_store.hpp"
#include "platform/browser.hpp"
#include "services/key_value_backend.hpp"
#include "services/save_quarantine.hpp"

#include <QString>
#include <functional>
#include <memory>

namespace heart_boom_maths::game
{

/// A store that can also keep a save it cannot read. Both the file store and the browser store are one, so main() holds
/// one object and hands the controller both of its interfaces.
class AppSaveStore : public SaveStore, public SaveQuarantine
{
};

/// Makes the store for `storage` (see platform::save_storage). `directory` is the folder of the file store, and
/// `make_browser_backend` makes the backend of the browser store, which a test replaces so it never touches real
/// settings. Nothing is read or written yet.
[[nodiscard]] std::unique_ptr<AppSaveStore>
make_save_store(platform::SaveStorage storage, const QString& directory,
                const std::function<std::unique_ptr<KeyValueBackend>()>& make_browser_backend);

/// The same with the native settings of QSettings as the browser backend.
[[nodiscard]] std::unique_ptr<AppSaveStore> make_save_store(platform::SaveStorage storage, const QString& directory);

} // namespace heart_boom_maths::game
