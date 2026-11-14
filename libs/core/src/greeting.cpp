// SPDX-License-Identifier: GPL-3.0-or-later
#include "heart_boom_maths/greeting.hpp"

#include <utility>

#include "heart_boom_maths/platform.hpp"

namespace heart_boom_maths
{

Greeter::Greeter(std::string name) : m_Name(std::move(name))
{
}

std::string Greeter::greet() const
{
    std::string text = "Hello, ";
    text += m_Name;
    text += ", from ";
    text += platform::host_name();
    text += ".";
    return text;
}

}  // namespace heart_boom_maths
