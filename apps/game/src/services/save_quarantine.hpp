// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace heart_boom_maths::game
{

/// What a store that keeps a file can do about a save it cannot read: move it aside so the next write does not
/// destroy it. The core's SaveStore only loads and stores text, so this is a separate, optional interface.
class SaveQuarantine
{
public:
    virtual ~SaveQuarantine() = default;

    /// Keeps the damaged save as save.json.bad. Returns false when it could not be moved.
    virtual bool set_damaged_aside() = 0;
};

} // namespace heart_boom_maths::game
