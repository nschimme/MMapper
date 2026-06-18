// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "CompareVersion.h"

#include <QRegularExpression>

CompareVersion::CompareVersion(const QString &versionStr) noexcept
{
    static const QRegularExpression versionRx(R"(v?(\d+)\.(\d+)\.(\d+))");
    auto result = versionRx.match(versionStr);
    if (result.hasMatch()) {
        m_parts[0] = result.captured(1).toInt();
        m_parts[1] = result.captured(2).toInt();
        m_parts[2] = result.captured(3).toInt();
    }
}

bool CompareVersion::operator>(const CompareVersion &other) const
{
    for (size_t i = 0; i < m_parts.size(); ++i) {
        auto myPart = m_parts.at(i);
        auto otherPart = other.m_parts.at(i);
        if (myPart == otherPart) {
            continue;
        }
        if (myPart > otherPart) {
            return true;
        }
        break;
    }
    return false;
}

bool CompareVersion::operator==(const CompareVersion &other) const
{
    return m_parts == other.m_parts;
}

QString CompareVersion::toQString() const
{
    const auto &parts = m_parts;
    return QString("%1.%2.%3").arg(parts.at(0)).arg(parts.at(1)).arg(parts.at(2));
}
