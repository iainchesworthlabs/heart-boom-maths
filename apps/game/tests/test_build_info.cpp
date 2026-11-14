// SPDX-License-Identifier: GPL-3.0-or-later
#include "heart_boom_maths/version.hpp"
#include "services/build_info.hpp"
#include "services/notices.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QString>

using namespace heart_boom_maths::game;

TEST_CASE("The generated build information describes this build", "[unit][build_info]")
{
    const BuildInfo info = BuildInfo::current();
    CHECK(info.app_name == "Heart Boom Maths");
    CHECK(info.version == std::string(heart_boom_maths::VERSION_STRING));
    CHECK(info.build_number >= 0);
    CHECK_FALSE(info.commit.empty());
    CHECK_FALSE(info.branch.empty());
    CHECK_FALSE(info.date.empty());
    CHECK(info.qt_version.starts_with("6."));
}

TEST_CASE("The footer reads name, version and build", "[unit][build_info]")
{
    BuildInfo info;
    info.app_name = "Heart Boom Maths";
    info.version = "1.0.0";
    info.build_number = 42;
    CHECK(info.footer() == "Heart Boom Maths 1.0.0 (build 42)");
}

TEST_CASE("The notices resource holds Qt and both font licences", "[unit][notices]")
{
    const QString text = notices_text();
    CHECK_FALSE(text.isEmpty());
    CHECK(text.contains(QStringLiteral("Qt 6.")));
    CHECK(text.contains(QStringLiteral("Bangers")));
    CHECK(text.contains(QStringLiteral("LilitaOne")));
    CHECK(text.contains(QStringLiteral("SIL OPEN FONT LICENSE")));
}
