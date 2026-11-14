// SPDX-License-Identifier: GPL-3.0-or-later
#import <GameKit/GameKit.h>
#import <UIKit/UIKit.h>

#include "platform/score_platform.hpp"
#include "services/game_center_backend.hpp"
#include "services/game_center_score_service.hpp"

#include <cstdint>
#include <memory>
#include <string_view>
#include <utility>

// The whole of the GameKit use of the game, and nothing in it decides anything: when to sign in and when to send is
// GameCenterScoreService's (plain C++, tested on every desktop). Three calls only: the local player's sign-in handler,
// its isAuthenticated flag and one score submission. The alias, the friends, the avatar and the player identifier are
// never asked for.

namespace heart_boom_maths::game::platform
{
namespace
{

/// The view controller that is on top of the screen now, which is where a sign-in sheet is shown from.
UIViewController* top_view_controller()
{
    UIWindow* window = nil;
    for (UIScene* scene in UIApplication.sharedApplication.connectedScenes)
    {
        if (scene.activationState == UISceneActivationStateForegroundActive &&
            [scene isKindOfClass:UIWindowScene.class])
        {
            window = static_cast<UIWindowScene*>(scene).keyWindow;
            break;
        }
    }
    UIViewController* top = window.rootViewController;
    while (top.presentedViewController != nil)
    {
        top = top.presentedViewController;
    }
    return top;
}

bool is_parental_block(NSError* error)
{
    return error != nil && [error.domain isEqualToString:GKErrorDomain] && error.code == GKErrorParentalControlsBlocked;
}

class GameKitBackend final : public GameCenterBackend
{
public:
    ~GameKitBackend() override
    {
        stop_authentication();
    }

    void start_authentication(StatusCallback on_status) override
    {
        // The handler runs now, again when the account changes and when the app comes back to the front. The status
        // goes to the main queue, which is where the service lives.
        m_Installed = true;
        GKLocalPlayer.localPlayer.authenticateHandler = ^(UIViewController* sign_in, NSError* error) {
          GameCenterStatus status;
          status.authenticated = GKLocalPlayer.localPlayer.isAuthenticated;
          status.restricted = is_parental_block(error);
          dispatch_async(dispatch_get_main_queue(), ^{
            // Shown only when GameKit hands over a sign-in sheet, which it does only while this handler is installed.
            if (sign_in != nil)
            {
                [top_view_controller() presentViewController:sign_in animated:YES completion:nil];
            }
            on_status(status);
          });
        };
    }

    void stop_authentication() override
    {
        // GameKit is not touched at all unless the handler was installed: a default install never reaches it.
        if (m_Installed)
        {
            m_Installed = false;
            GKLocalPlayer.localPlayer.authenticateHandler = nil;
        }
    }

    void submit_score(std::int64_t score, std::string_view leaderboard_id) override
    {
        NSString* identifier = [[NSString alloc] initWithBytes:leaderboard_id.data()
                                                        length:leaderboard_id.size()
                                                      encoding:NSUTF8StringEncoding];
        [GKLeaderboard submitScore:static_cast<NSInteger>(score)
                           context:0
                            player:GKLocalPlayer.localPlayer
                    leaderboardIDs:@[ identifier ]
                 completionHandler:^(NSError* error) {
                   // Only the error code is logged: the log is not a place for anything about the player.
                   if (error != nil)
                   {
                       NSLog(@"Game Center: the score was not sent (error %ld)", static_cast<long>(error.code));
                   }
                 }];
    }

private:
    bool m_Installed = false;
};

} // namespace

std::unique_ptr<ScoreService> make_score_service()
{
    return std::make_unique<GameCenterScoreService>(std::make_unique<GameKitBackend>());
}

} // namespace heart_boom_maths::game::platform
