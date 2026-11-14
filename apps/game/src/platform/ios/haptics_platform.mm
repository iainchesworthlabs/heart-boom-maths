// SPDX-License-Identifier: GPL-3.0-or-later
#import <CoreHaptics/CoreHaptics.h>
#import <UIKit/UIKit.h>

#include "platform/haptics_platform.hpp"
#include "services/haptic_maps.hpp"

#include <memory>

// The whole of the UIKit haptics of the game, and nothing in it decides anything: the generator for each beat is
// ios_feedback() (plain C++, tested on every desktop) and whether and when one plays is BackedHapticsService's. The
// feedback generators ask for no permission and call no API that Apple's privacy manifest lists.
//
// available() is CoreHaptics' answer for this device ([CHHapticEngine capabilitiesForHardware].supportsHaptics): true
// on an iPhone with a Taptic Engine, false on an iPad, which has none, and on the iPhones that predate it. The
// generators would do nothing there, and Settings would offer a switch that does nothing.

namespace heart_boom_maths::game::platform
{
namespace
{

class UiKitHapticBackend final : public HapticBackend
{
public:
    UiKitHapticBackend() : m_Available([CHHapticEngine capabilitiesForHardware].supportsHaptics)
    {
        if (!m_Available)
        {
            return;
        }
        // The generators belong to the main thread, which is where the application starts this.
        m_Selection = [[UISelectionFeedbackGenerator alloc] init];
        m_Notification = [[UINotificationFeedbackGenerator alloc] init];
        m_Soft = [[UIImpactFeedbackGenerator alloc] initWithStyle:UIImpactFeedbackStyleSoft];
        m_Light = [[UIImpactFeedbackGenerator alloc] initWithStyle:UIImpactFeedbackStyleLight];
        m_Rigid = [[UIImpactFeedbackGenerator alloc] initWithStyle:UIImpactFeedbackStyleRigid];
        m_Medium = [[UIImpactFeedbackGenerator alloc] initWithStyle:UIImpactFeedbackStyleMedium];
        m_Heavy = [[UIImpactFeedbackGenerator alloc] initWithStyle:UIImpactFeedbackStyleHeavy];
        // The two that answers and taps use most are ready from the start.
        [m_Selection prepare];
        [m_Rigid prepare];
    }

    [[nodiscard]] bool available() const override
    {
        return m_Available;
    }

    void fire(HapticId beat) override
    {
        if (!m_Available)
        {
            return;
        }
        switch (ios_feedback(beat))
        {
        case IosFeedback::SELECTION:
            [m_Selection selectionChanged];
            [m_Selection prepare];
            break;
        case IosFeedback::IMPACT_SOFT:
            impact(m_Soft);
            break;
        case IosFeedback::IMPACT_LIGHT:
            impact(m_Light);
            break;
        case IosFeedback::IMPACT_RIGID:
            impact(m_Rigid);
            break;
        case IosFeedback::IMPACT_MEDIUM:
            impact(m_Medium);
            break;
        case IosFeedback::IMPACT_HEAVY:
            impact(m_Heavy);
            break;
        case IosFeedback::NOTIFY_SUCCESS:
            notify(UINotificationFeedbackTypeSuccess);
            break;
        case IosFeedback::NOTIFY_WARNING:
            notify(UINotificationFeedbackTypeWarning);
            break;
        case IosFeedback::NOTIFY_ERROR:
            notify(UINotificationFeedbackTypeError);
            break;
        }
    }

private:
    // A generator is prepared again after each use, so the engine is awake for the next beat of the same kind.
    static void impact(UIImpactFeedbackGenerator* generator)
    {
        [generator impactOccurred];
        [generator prepare];
    }

    void notify(UINotificationFeedbackType type)
    {
        [m_Notification notificationOccurred:type];
        [m_Notification prepare];
    }

    bool m_Available;
    UISelectionFeedbackGenerator* m_Selection = nil;
    UINotificationFeedbackGenerator* m_Notification = nil;
    UIImpactFeedbackGenerator* m_Soft = nil;
    UIImpactFeedbackGenerator* m_Light = nil;
    UIImpactFeedbackGenerator* m_Rigid = nil;
    UIImpactFeedbackGenerator* m_Medium = nil;
    UIImpactFeedbackGenerator* m_Heavy = nil;
};

} // namespace

std::unique_ptr<HapticBackend> make_haptic_backend()
{
    return std::make_unique<UiKitHapticBackend>();
}

} // namespace heart_boom_maths::game::platform
