// SPDX-License-Identifier: GPL-3.0-or-later
#import <Foundation/Foundation.h>

#include "platform/cloud_platform.hpp"
#include "services/cloud_store.hpp"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

// The whole of the iCloud use of the game, and the only file that names a cloud API. It holds the one text the game
// syncs under one key of NSUbiquitousKeyValueStore.defaultStore, and decides nothing: when to read, merge and write is
// plain C++ (services/cloud_sync.*, tested on every desktop). The game has no account, sign-in or server of its own;
// the system's iCloud daemon moves the key between the devices signed in to one Apple Account.
//
// Nothing here reaches the store until the game asks. Making the backend, and available(), touch no key; the store is
// first used by set_change_handler(non-empty), read() or write(), which the controller calls only while the Settings
// switch is on. No call here waits for the network: the store keeps its values in memory and hands them to iCloud
// asynchronously.
//
// Not yet compiled: this file was written without a Mac (docs/ios.md says so).

namespace heart_boom_maths::game::platform
{
namespace
{

/// The one key. Well under the 64 bytes Apple's design guide allows for a key.
NSString* const PAYLOAD_KEY = @"hbm.shared.v2";

/// A property list key that CMake fills in from HEART_BOOM_MATHS_IOS_ICLOUD (ios/ios.cmake): true when the bundle was
/// built asking for the key-value store entitlement. Without that entitlement the store silently keeps nothing, so the
/// Settings switch must not be offered.
NSString* const ENTITLED_INFO_KEY = @"HBMICloudSyncEntitled";

/// What the observer block shares with the backend. Used on the main thread only.
struct Shared
{
    CloudStore::ChangeHandler handler;
};

class UbiquitousKeyValueStore final : public CloudStore
{
public:
    ~UbiquitousKeyValueStore() override
    {
        // A notice that is already queued finds no handler.
        m_Shared->handler = nullptr;
        remove_observer();
    }

    [[nodiscard]] bool available() const override
    {
        // The bundle asked for the entitlement, and an iCloud account is signed in. ubiquityIdentityToken is nil when
        // iCloud is unavailable or nobody is logged in; reading it is fast and does not read the store.
        NSNumber* entitled = [NSBundle.mainBundle objectForInfoDictionaryKey:ENTITLED_INFO_KEY];
        if (![entitled isKindOfClass:NSNumber.class] || !entitled.boolValue)
        {
            return false;
        }
        return NSFileManager.defaultManager.ubiquityIdentityToken != nil;
    }

    [[nodiscard]] std::optional<std::string> read() override
    {
        // stringForKey: is nil when the key is absent and also when the value is not a string.
        NSString* text = [NSUbiquitousKeyValueStore.defaultStore stringForKey:PAYLOAD_KEY];
        if (text == nil)
        {
            return std::nullopt;
        }
        const NSUInteger length = [text lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
        const char* bytes = text.UTF8String;
        if (bytes == nullptr)
        {
            return std::nullopt;
        }
        return std::string(bytes, length);
    }

    bool write(std::string_view text) override
    {
        NSString* value = [[NSString alloc] initWithBytes:text.data() length:text.size() encoding:NSUTF8StringEncoding];
        if (value == nil)
        {
            return false;
        }
        // Kept in memory and uploaded by the system later; a value over the quota is refused by the system, which
        // reports it through the notification below.
        [NSUbiquitousKeyValueStore.defaultStore setString:value forKey:PAYLOAD_KEY];
        return true;
    }

    void set_change_handler(ChangeHandler handler) override
    {
        const bool wanted = static_cast<bool>(handler);
        m_Shared->handler = std::move(handler);
        if (wanted && m_Observer == nil)
        {
            install_observer();
        }
        else if (!wanted)
        {
            remove_observer();
        }
    }

private:
    void install_observer()
    {
        const std::shared_ptr<Shared> shared = m_Shared;
        // The main queue: the block runs on the application's main thread whatever thread iCloud's notice arrives on,
        // and that is the Qt GUI thread on iOS.
        m_Observer = [NSNotificationCenter.defaultCenter
            addObserverForName:NSUbiquitousKeyValueStoreDidChangeExternallyNotification
                        object:NSUbiquitousKeyValueStore.defaultStore
                         queue:NSOperationQueue.mainQueue
                    usingBlock:^(NSNotification* notification) {
                      NSNumber* reason = notification.userInfo[NSUbiquitousKeyValueStoreChangeReasonKey];
                      if (reason != nil && reason.integerValue == NSUbiquitousKeyValueStoreQuotaViolationChange)
                      {
                          // Nothing new came in; a write was refused. The log says no more than that.
                          NSLog(@"iCloud sync: the store is over its quota");
                          return;
                      }
                      // A server change, the first sync and a change of account all mean the text may differ now.
                      const CloudStore::ChangeHandler handler = shared->handler;
                      if (handler)
                      {
                          handler();
                      }
                    }];
        // Apple's design guide: register for the notification, then synchronize once, to pick up changes made while the
        // app was not running.
        [NSUbiquitousKeyValueStore.defaultStore synchronize];
    }

    void remove_observer()
    {
        if (m_Observer != nil)
        {
            [NSNotificationCenter.defaultCenter removeObserver:m_Observer];
            m_Observer = nil;
        }
    }

    std::shared_ptr<Shared> m_Shared = std::make_shared<Shared>();
    id<NSObject> m_Observer = nil;
};

} // namespace

std::unique_ptr<CloudStore> make_cloud_store()
{
    return std::make_unique<UbiquitousKeyValueStore>();
}

} // namespace heart_boom_maths::game::platform
