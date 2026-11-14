// SPDX-License-Identifier: GPL-3.0-or-later
#include "services/system_link_opener.hpp"

#include <QDesktopServices>
#include <QString>
#include <QUrl>

namespace heart_boom_maths::game
{

void SystemLinkOpener::open(const std::string& url)
{
    QDesktopServices::openUrl(QUrl(QString::fromStdString(url)));
}

} // namespace heart_boom_maths::game
