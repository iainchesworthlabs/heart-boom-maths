// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "services/link_opener.hpp"

namespace heart_boom_maths::game
{

/// Hands the address to the system browser. This is the one place in the game that opens a URL;
/// tools/ci/check_link_open.py counts it.
class SystemLinkOpener final : public LinkOpener
{
public:
    void open(const std::string& url) override;
};

} // namespace heart_boom_maths::game
