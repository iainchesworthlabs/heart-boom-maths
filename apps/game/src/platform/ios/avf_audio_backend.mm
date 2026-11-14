// SPDX-License-Identifier: GPL-3.0-or-later
#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>

#include "services/audio_backend.hpp"
#include "services/audio_resource.hpp"
#include "services/clip_audio_backend.hpp"

#include <cstddef>
#include <memory>
#include <span>
#include <string_view>
#include <utility>

// The whole of the AVFoundation use of the game for sound, and nothing in it decides anything: which clips are loaded,
// when the session is activated, what an interruption stops and what comes back are ClipAudioBackend's (plain C++,
// tested on every desktop). This file only turns those calls into AVAudioPlayer and AVAudioSession calls, and the three
// session notifications into calls of the backend. It reads no file, plays nothing from a network and asks for no
// permission.

namespace heart_boom_maths::game
{
namespace
{

/// An AVAudioPlayer that is made once and played again and again. stop() prepares it again, so the next play is quick.
class AvfPlayer final : public ClipPlayer
{
public:
    explicit AvfPlayer(AVAudioPlayer* player) : m_Player(player) {}

    void play_from_start() override
    {
        m_Player.currentTime = 0;
        [m_Player play];
    }

    void stop() override
    {
        [m_Player stop];
        m_Player.currentTime = 0;
        [m_Player prepareToPlay];
    }

private:
    AVAudioPlayer* m_Player;
};

/// The audio session of the app and the player factory. Ambient follows the silent switch, mixes with the music the
/// player has on, and is the category for sound that is not the point of the app. Nothing is recorded, so no microphone
/// use.
class AvfHost final : public PlayerHost
{
public:
    AvfHost()
    {
        // Set now, not activated: the first AVAudioPlayer must not find the default category (which ignores the
        // switch).
        NSError* error = nil;
        if (![AVAudioSession.sharedInstance setCategory:AVAudioSessionCategoryAmbient error:&error])
        {
            NSLog(@"audio: the session category was not set (error %ld)", static_cast<long>(error.code));
        }
    }

    bool activate_session() override
    {
        AVAudioSession* session = AVAudioSession.sharedInstance;
        NSError* error = nil;
        // The category again, because a reset of the media services drops it.
        if (![session setCategory:AVAudioSessionCategoryAmbient error:&error] || ![session setActive:YES error:&error])
        {
            NSLog(@"audio: the session was not activated (error %ld)", static_cast<long>(error.code));
            return false;
        }
        return true;
    }

    std::unique_ptr<ClipPlayer> make_player(std::span<const std::byte> sound_file, bool looping, float volume) override
    {
        NSData* data = [NSData dataWithBytes:sound_file.data() length:sound_file.size()];
        NSError* error = nil;
        AVAudioPlayer* player = [[AVAudioPlayer alloc] initWithData:data fileTypeHint:AVFileTypeWAVE error:&error];
        if (player == nil)
        {
            NSLog(@"audio: a player was not made (error %ld)", static_cast<long>(error.code));
            return nullptr;
        }
        player.numberOfLoops = looping ? -1 : 0;
        player.volume = volume;
        [player prepareToPlay];
        return std::make_unique<AvfPlayer>(player);
    }
};

/// The two kinds of interruption notification, as the calls the backend decides on.
void report_interruption(ClipAudioBackend& backend, NSNotification* note)
{
    const NSUInteger type = [note.userInfo[AVAudioSessionInterruptionTypeKey] unsignedIntegerValue];
    if (type == AVAudioSessionInterruptionTypeBegan)
    {
        backend.interruption_began();
    }
    else if (type == AVAudioSessionInterruptionTypeEnded)
    {
        const NSUInteger options = [note.userInfo[AVAudioSessionInterruptionOptionKey] unsignedIntegerValue];
        backend.interruption_ended((options & AVAudioSessionInterruptionOptionShouldResume) != 0);
    }
}

/// ClipAudioBackend plus the three notifications of the audio session. They arrive on the main queue, which is where
/// the game's audio service lives.
class AvfAudioBackend final : public ClipAudioBackend
{
public:
    AvfAudioBackend() : ClipAudioBackend(std::make_unique<AvfHost>(), read_audio_clip, log_line)
    {
        AvfAudioBackend* const backend = this;
        NSNotificationCenter* const center = NSNotificationCenter.defaultCenter;
        NSOperationQueue* const main_queue = NSOperationQueue.mainQueue;
        AVAudioSession* const session = AVAudioSession.sharedInstance;

        m_Observers = [NSMutableArray array];
        [m_Observers addObject:[center addObserverForName:AVAudioSessionInterruptionNotification
                                                   object:session
                                                    queue:main_queue
                                               usingBlock:^(NSNotification* note) {
                                                 report_interruption(*backend, note);
                                               }]];
        [m_Observers addObject:[center addObserverForName:AVAudioSessionMediaServicesWereResetNotification
                                                   object:session
                                                    queue:main_queue
                                               usingBlock:^(NSNotification*) {
                                                 backend->media_services_reset();
                                               }]];
    }

    ~AvfAudioBackend() override
    {
        for (id observer in m_Observers)
        {
            [NSNotificationCenter.defaultCenter removeObserver:observer];
        }
    }

    AvfAudioBackend(const AvfAudioBackend&) = delete;
    AvfAudioBackend& operator=(const AvfAudioBackend&) = delete;

private:
    // A line about a clip, never about the player.
    static void log_line(std::string_view line)
    {
        NSLog(@"%.*s", static_cast<int>(line.size()), line.data());
    }

    NSMutableArray* m_Observers;
};

} // namespace

std::unique_ptr<AudioBackend> make_device_audio_backend()
{
    auto backend = std::make_unique<AvfAudioBackend>();
    // Not one clip could be loaded: a silent game is better than a backend that does nothing.
    if (backend->loaded_clips() == 0)
    {
        return nullptr;
    }
    return backend;
}

} // namespace heart_boom_maths::game
