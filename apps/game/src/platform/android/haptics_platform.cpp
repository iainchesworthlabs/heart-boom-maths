// SPDX-License-Identifier: GPL-3.0-or-later
#include "platform/haptics_platform.hpp"

#include "services/haptic_maps.hpp"

#include <QCoreApplication>
#include <QJniEnvironment>
#include <QJniObject>
#include <QVariant>
#include <memory>

// The whole of the Android haptics, and nothing in it decides anything: the constant for each beat is
// android_feedback_constant() (plain C++, tested on every desktop) and whether and when a beat plays is
// BackedHapticsService's. The call is View.performHapticFeedback(int) on the window's decor view, which needs no
// permission (the manifest asks for none and a test of the package fails on any). It is made without flags, so the
// system's own touch-feedback switch and the view's setting both still decide whether the phone moves.

namespace heart_boom_maths::game::platform
{
namespace
{

/// Context.VIBRATOR_SERVICE. Asked only whether the device has a motor (Vibrator.hasVibrator), which needs no
/// permission either; vibrate() is never called, because that is the call that needs android.permission.VIBRATE.
constexpr auto VIBRATOR_SERVICE = "vibrator";

bool device_has_vibrator()
{
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid())
    {
        return false;
    }
    const QJniObject service =
        context.callObjectMethod("getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;",
                                 QJniObject::fromString(QLatin1String(VIBRATOR_SERVICE)).object());
    bool has = false;
    if (service.isValid())
    {
        has = service.callMethod<jboolean>("hasVibrator") != JNI_FALSE;
    }
    // A failed call must not leave an exception pending in the Java side of the application.
    QJniEnvironment env;
    if (env.checkAndClearExceptions())
    {
        return false;
    }
    return has;
}

class PerformHapticFeedbackBackend final : public HapticBackend
{
public:
    PerformHapticFeedbackBackend() : m_Available(device_has_vibrator()) {}

    [[nodiscard]] bool available() const override
    {
        return m_Available;
    }

    void fire(HapticId beat) override
    {
        const int constant = android_feedback_constant(beat, QNativeInterface::QAndroidApplication::sdkVersion());
        // The view belongs to Android's UI thread, which is not Qt's. The call is not waited for.
        QNativeInterface::QAndroidApplication::runOnAndroidMainThread(
            [constant]
            {
                QJniObject activity = QNativeInterface::QAndroidApplication::context();
                if (!activity.isValid())
                {
                    return;
                }
                const QJniObject window = activity.callObjectMethod("getWindow", "()Landroid/view/Window;");
                const QJniObject decor =
                    window.isValid() ? window.callObjectMethod("getDecorView", "()Landroid/view/View;") : QJniObject();
                if (decor.isValid())
                {
                    decor.callMethod<jboolean>("performHapticFeedback", "(I)Z", static_cast<jint>(constant));
                }
                QJniEnvironment env;
                env.checkAndClearExceptions();
            });
    }

private:
    bool m_Available;
};

} // namespace

std::unique_ptr<HapticBackend> make_haptic_backend()
{
    return std::make_unique<PerformHapticFeedbackBackend>();
}

} // namespace heart_boom_maths::game::platform
