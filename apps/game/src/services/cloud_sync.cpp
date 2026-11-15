// SPDX-License-Identifier: GPL-3.0-or-later
#include "services/cloud_sync.hpp"

#include "heart_boom_maths/save_codec.hpp"
#include "heart_boom_maths/save_merge.hpp"

#include <optional>
#include <utility>

namespace heart_boom_maths::game
{

namespace
{

CloudSyncResult refused(std::string_view reason)
{
    return {.outcome = CloudSyncOutcome::REFUSED, .write_failed = false, .reason = reason};
}

/// The cloud takes this device's shared part as it is, because it held nothing or nothing this build can read.
CloudSyncResult replace_cloud_copy(CloudStore& cloud, const SharedData& shared, const Tuning& tuning)
{
    const auto text = serialise_shared(shared, tuning);
    if (!text.has_value())
    {
        return refused("the local players and scores are not valid, so they were not sent");
    }
    if (text->size() > CLOUD_PAYLOAD_LIMIT_BYTES)
    {
        return refused("the players and scores are too large to send");
    }
    if (!cloud.write(*text))
    {
        return {.outcome = CloudSyncOutcome::NOTHING_TO_DO, .write_failed = true, .reason = {}};
    }
    return {.outcome = CloudSyncOutcome::CLOUD_CHANGED, .write_failed = false, .reason = {}};
}

CloudSyncOutcome outcome_of(bool local_changed, bool cloud_changed) noexcept
{
    if (local_changed && cloud_changed)
    {
        return CloudSyncOutcome::BOTH_CHANGED;
    }
    if (local_changed)
    {
        return CloudSyncOutcome::LOCAL_CHANGED;
    }
    return cloud_changed ? CloudSyncOutcome::CLOUD_CHANGED : CloudSyncOutcome::NOTHING_TO_DO;
}

} // namespace

CloudSyncResult sync_with_cloud(CloudStore& cloud, SaveData& local,
                                const std::function<std::string(const SharedData&)>& new_player_id,
                                const Tuning& tuning)
{
    const std::optional<std::string> stored = cloud.read();
    if (!stored.has_value())
    {
        return replace_cloud_copy(cloud, local.shared, tuning);
    }

    SharedParseResult parsed = parse_shared(*stored, tuning);
    if (parsed.status == LoadStatus::NEWER)
    {
        return {.outcome = CloudSyncOutcome::SKIPPED_NEWER, .write_failed = false, .reason = {}};
    }
    if (parsed.status == LoadStatus::DAMAGED)
    {
        return replace_cloud_copy(cloud, local.shared, tuning);
    }

    auto merged = merge(local.shared, parsed.data, tuning);
    if (!merged.has_value())
    {
        return refused("the players and scores of the two devices cannot be joined (a format limit)");
    }
    // Everything that can refuse comes before anything changes, so a refusal leaves the local data as it was.
    auto text = serialise_shared(*merged, tuning);
    if (!text.has_value())
    {
        return refused("the joined players and scores are not valid");
    }
    if (text->size() > CLOUD_PAYLOAD_LIMIT_BYTES)
    {
        return refused("the joined players and scores are too large to send");
    }

    const bool local_changed = *merged != local.shared;
    if (local_changed)
    {
        // A player is made up only when the merge leaves nobody (each device removed a different player). That player
        // goes to the cloud as well, or every device would make up its own.
        const bool made_up_player = active_player_count(*merged) == 0;
        std::string spare_id = made_up_player ? new_player_id(*merged) : std::string();
        if (!adopt_shared(local, std::move(*merged), std::move(spare_id), tuning).has_value())
        {
            return refused("the joined players could not be taken in");
        }
        if (made_up_player)
        {
            text = serialise_shared(local.shared, tuning);
            if (!text.has_value() || text->size() > CLOUD_PAYLOAD_LIMIT_BYTES)
            {
                return {.outcome = CloudSyncOutcome::LOCAL_CHANGED, .write_failed = true, .reason = {}};
            }
        }
    }

    const bool cloud_changed = *text != *stored;
    bool write_failed = false;
    if (cloud_changed)
    {
        write_failed = !cloud.write(*text);
    }
    return {.outcome = outcome_of(local_changed, cloud_changed && !write_failed),
            .write_failed = write_failed,
            .reason = {}};
}

} // namespace heart_boom_maths::game
