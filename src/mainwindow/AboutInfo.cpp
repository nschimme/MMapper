// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "AboutInfo.h"

#include "../global/ConfigConsts-Computed.h"
#include "../global/ConfigEnums.h"
#include "../global/Version.h"

#include <QFile>
#include <QIODevice>
#include <QSysInfo>
#include <QTextStream>
#include <QVariantMap>

namespace { // anonymous

struct NODISCARD LicenseInfo final
{
    QString title;
    QString introText;
    QString resourcePath;
};

NODISCARD QString getBuildInformation()
{
    const auto get_compiler = []() -> QString {
#ifdef __clang__
        return QString("Clang %1.%2.%3")
            .arg(__clang_major__)
            .arg(__clang_minor__)
            .arg(__clang_patchlevel__);
#elif __GNUC__
        return QString("GCC %1.%2.%3").arg(__GNUC__).arg(__GNUC_MINOR__).arg(__GNUC_PATCHLEVEL__);
#elif defined(_MSC_VER)
        return QString("MSVC %1").arg(_MSC_VER);
#else
        return "an unknown compiler";
#endif
    };

    return QString("Built on branch %1 using %2<br>")
        .arg(QString::fromUtf8(getMMapperBranch()), get_compiler());
}

NODISCARD QString loadResource(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString("Unable to open resource '%1'.").arg(path);
    } else {
        QTextStream ts(&f);
        return ts.readAll();
    }
}

} // namespace

void LicenseModel::setEntries(QVector<Entry> entries)
{
    beginResetModel();
    m_entries = std::move(entries);
    endResetModel();
}

int LicenseModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_entries.size());
}

QVariant LicenseModel::data(const QModelIndex &index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_entries.size())) {
        return QVariant();
    }
    const Entry &entry = m_entries.at(index.row());
    switch (role) {
    case TitleRole:
        return entry.title;
    case IntroRole:
        return entry.intro;
    case TextRole:
        return entry.text;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> LicenseModel::roleNames() const
{
    return {{TitleRole, "title"}, {IntroRole, "intro"}, {TextRole, "text"}};
}

AboutInfo::AboutInfo(QObject *const parent)
    : QObject(parent)
    , m_licenses(new LicenseModel(this))
{
    /* About tab */
    m_aboutHtml = "<p align=\"center\">"
                  "<h3>"
                  "<u>"
                  + QString("MMapper %1").arg(QString::fromUtf8(getMMapperVersion()))
                  + "</h3>"
                    "</u>"
                    "</p>"
                    "<p align=\"center\">"
                  + getBuildInformation()
                  + QString("Based on Qt %1 (%2 bit)")
                        .arg(qVersion())
                        .arg(static_cast<size_t>(QSysInfo::WordSize))
                  + "</p>";

    /* Authors tab */
    const QStringList authors = loadResource(":/AUTHORS.txt").split("\n");
    if (authors.isEmpty()) {
        m_authorsHtml = "Could not load authors file.";
    } else {
        QString authorsHtml
            = "<p>The MMapper project is maintained by the following contributors:</p><ul>";
        for (const auto &line : authors) {
            authorsHtml += "<li>" + line + "</li></br>";
        }
        authorsHtml += "</ul>";
        m_authorsHtml = authorsHtml;
    }

    /* Licenses tab */
    QList<LicenseInfo> licenses;
    licenses.append({"GNU General Public License 2.0",
                     "<p>"
                     "This program is free software; you can redistribute it and/or "
                     "modify it under the terms of the GNU General Public License "
                     "as published by the Free Software Foundation; either version 2 "
                     "of the License, or (at your option) any later version."
                     "</p>"
                     "<p>"
                     "This program is distributed in the hope that it will be useful, "
                     "but WITHOUT ANY WARRANTY; without even the implied warranty of "
                     "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE."
                     "</p>"
                     "<p>"
                     "See the GNU General Public License for more details. "
                     "</p>",
                     ":/LICENSE.GPL2"});

    licenses.append({"DejaVu Fonts License",
                     "<p>"
                     "This license applies to the file "
                     "<code>src/resources/fonts/DejaVuSansMono.ttf</code>"
                     "</p>",
                     ":/fonts/LICENSE"});

    licenses.append({"GLM License",
                     "<p>"
                     "This product contains code from the "
                     "<a href=\"https://glm.g-truc.net/\">OpenGL Mathematics (GLM)</a>"
                     " project."
                     "</p>",
                     ":/LICENSE.GLM"});

    licenses.append({"QtKeychain License",
                     "<p>"
                     "This product contains code from the "
                     "<a href=\"https://github.com/frankosterfeld/qtkeychain\">QtKeychain</a>"
                     " project."
                     "</p>",
                     ":/LICENSE.QTKEYCHAIN"});

    licenses.append({"OpenSSL License",
                     "<p>"
                     "Some versions of this product contains code from the "
                     "<a href=\"https://www.openssl.org/\">OpenSSL toolkit</a>."
                     "</p>",
                     ":/LICENSE.OPENSSL"});

    licenses.append({"Boost Software License 1.0",
                     "<p>"
                     "This product contains code from the "
                     "<a href=\"https://github.com/arximboldi/immer\">immer</a>"
                     " project."
                     "</p>",
                     ":/LICENSE.BOOST"});

    if constexpr (CURRENT_PLATFORM == PlatformEnum::Windows) {
        licenses.append({"GNU Lesser General Public License 2.1",
                         "<p>"
                         "Some versions of this product contains code from the "
                         "following LGPLed libraries: "
                         "<a href=\"https://github.com/jrfonseca/drmingw\">DrMingW</a>"
                         "</p>",
                         ":/LICENSE.LGPL"});
    }

    QVector<LicenseModel::Entry> entries;
    entries.reserve(licenses.size());
    for (const auto &license : licenses) {
        entries.append(LicenseModel::Entry{license.title,
                                           license.introText,
                                           loadResource(license.resourcePath)});
    }
    m_licenses->setEntries(std::move(entries));
}
