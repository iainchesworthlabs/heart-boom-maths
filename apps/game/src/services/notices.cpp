// SPDX-License-Identifier: GPL-3.0-or-later
#include "services/notices.hpp"

#include <QFile>

namespace heart_boom_maths::game
{

QString notices_text()
{
    QFile file(QStringLiteral(":/heart_boom_maths/notices.txt"));
    if (!file.open(QIODevice::ReadOnly))
    {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

} // namespace heart_boom_maths::game
