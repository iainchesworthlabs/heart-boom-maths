// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "services/haptics_service.hpp"

namespace heart_boom_maths::game
{

/// The device end of haptics: one beat at a time and nothing else. Every decision (whether, when, how often) is
/// BackedHapticsService's, in plain C++ that a test drives with a fake; an implementation under src/platform/<os>/ only
/// translates a beat into the calls of its system.
class HapticBackend
{
public:
    virtual ~HapticBackend() = default;

    /// Whether this device can make a haptic at all. Asked often; it must be cheap.
    [[nodiscard]] virtual bool available() const = 0;

    /// One beat now. WIN is a run of beats that the service has already split, so it never arrives here; a backend
    /// answers it like LEVEL_CLEAR if it does.
    virtual void fire(HapticId beat) = 0;

    /// Stops what the device may still be doing from an earlier beat, where the system allows it.
    virtual void cancel() {}
};

/// The backend of a platform with no haptics.
class NullHapticBackend final : public HapticBackend
{
public:
    [[nodiscard]] bool available() const override
    {
        return false;
    }

    void fire(HapticId) override {}
};

} // namespace heart_boom_maths::game
