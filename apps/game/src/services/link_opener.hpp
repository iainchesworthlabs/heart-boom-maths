// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>

namespace heart_boom_maths::game
{

/// Opens a web address outside the game. Only GameController calls it, and only after the parental gate is passed.
class LinkOpener
{
public:
    virtual ~LinkOpener() = default;

    virtual void open(const std::string& url) = 0;
};

} // namespace heart_boom_maths::game
