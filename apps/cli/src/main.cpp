// SPDX-License-Identifier: GPL-3.0-or-later
#include <cstdio>
#include <string>
#include <string_view>

#include "heart_boom_maths/greeting.hpp"
#include "heart_boom_maths/version.hpp"

int main(int argc, char** argv)
{
    if (argc == 2 && std::string_view(argv[1]) == "--version")
    {
        std::printf("heart-boom-maths %s\n", std::string(heart_boom_maths::VERSION_STRING).c_str());
        return 0;
    }
    if (argc > 2)
    {
        std::fprintf(stderr, "usage: heart-boom-maths [NAME | --version]\n");
        return 2;
    }

    const heart_boom_maths::Greeter greeter(argc == 2 ? argv[1] : "world");
    std::printf("%s\n", greeter.greet().c_str());
    return 0;
}
