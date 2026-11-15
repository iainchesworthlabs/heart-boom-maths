// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "heart_boom_maths/save_data.hpp"
#include "heart_boom_maths/tuning.hpp"
#include "services/cloud_store.hpp"

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>

namespace heart_boom_maths::game
{

/// The most text one sync writes. Apple's key-value store takes 1 MB for a value and 1 MB for the whole app; a real
/// payload is a few kilobytes, and one that would come near is refused and not sent (docs/ios.md).
inline constexpr std::size_t CLOUD_PAYLOAD_LIMIT_BYTES = 900'000;

/// What one sync found and did, so the caller knows what to tell the screens.
enum class CloudSyncOutcome
{
    NOTHING_TO_DO, // the cloud copy and the local data already agree
    LOCAL_CHANGED, // the cloud had something this device lacked: the local save took it, and the cloud copy was current
    CLOUD_CHANGED, // this device had something the cloud lacked (or the cloud held nothing, or nothing readable):
                   // written
    BOTH_CHANGED,  // each had something the other lacked
    SKIPPED_NEWER, // the cloud copy was written by a later build: left alone, not merged, not written (decision P7)
    REFUSED,       // nothing was done: see CloudSyncResult::reason
};

struct CloudSyncResult
{
    CloudSyncOutcome outcome = CloudSyncOutcome::NOTHING_TO_DO;
    /// The store did not take a write. The local data is as the outcome says; the next sync tries again.
    bool write_failed = false;
    /// For REFUSED: a plain sentence for the log (never data). Empty otherwise.
    std::string_view reason;

    [[nodiscard]] bool local_changed() const noexcept
    {
        return outcome == CloudSyncOutcome::LOCAL_CHANGED || outcome == CloudSyncOutcome::BOTH_CHANGED;
    }
};

/// One sync of the shared part of `local` with the cloud copy. No Qt and no clock: the rules over a CloudStore, which a
/// test fakes.
///
///  - No payload: the local shared part is written.
///  - A payload of a later version (NEWER): left alone. Nothing is merged or written (decision P7).
///  - A payload that cannot be read (DAMAGED): replaced by the local shared part.
///  - Otherwise the core's `merge` joins the two. A merge that breaks a format bound keeps the local data and writes
///    nothing (REFUSED). Else the merged part goes into `local` through the core's `adopt_shared`, and the merged text
///    is written back only when it differs from the text that was read, so two devices in step stop writing.
///
/// The caller records the session's wallet in `local` first and takes the player's wallet back into the session after a
/// sync that changed it: this function knows nothing of a session. `new_player_id` is asked for only when the merge
/// leaves no active player at all, and must give 16 random hex digits (`hex_id`).
[[nodiscard]] CloudSyncResult sync_with_cloud(CloudStore& cloud, SaveData& local,
                                              const std::function<std::string()>& new_player_id,
                                              const Tuning& tuning = Tuning::defaults());

} // namespace heart_boom_maths::game
