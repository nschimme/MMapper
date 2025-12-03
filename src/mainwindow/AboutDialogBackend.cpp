// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "AboutDialogBackend.h"
#include "../global/ConfigConsts-Computed.h"
#include "../global/ConfigEnums.h"
#include "../global/Version.h"

#include <QString>
#include <QtConfig>
#include <QtGui>
#include <QtWidgets>

namespace {

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

} // namespace

AboutDialogBackend::AboutDialogBackend(QObject *parent) : QObject(parent)
{
    const auto loadResource = [](const QString &path) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return QString("Unable to open resource '%1'.").arg(path);
        } else {
            QTextStream ts(&f);
            return ts.readAll();
        }
    };

    m_licenses.append(new LicenseInfo("GNU General Public License 2.0",
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
                     loadResource(":/LICENSE.GPL2"), this));

    m_licenses.append(new LicenseInfo("DejaVu Fonts License",
                     "<p>"
                     "This license applies to the file "
                     "<code>src/resources/fonts/DejaVuSansMono.ttf</code>"
                     "</p>",
                     loadResource(":/fonts/LICENSE"), this));

    m_licenses.append(new LicenseInfo("GLM License",
                     "<p>"
                     "This product contains code from the "
                     "<a href=\"https://glm.g-truc.net/\">OpenGL Mathematics (GLM)</a>"
                     " project."
                     "</p>",
                     loadResource(":/LICENSE.GLM"), this));

    m_licenses.append(new LicenseInfo("QtKeychain License",
                     "<p>"
                     "This product contains code from the "
                     "<a href=\"https://github.com/frankosterfeld/qtkeychain\">QtKeychain</a>"
                     " project."
                     "</p>",
                     loadResource(":/LICENSE.QTKEYCHAIN"), this));

    m_licenses.append(new LicenseInfo("OpenSSL License",
                     "<p>"
                     "Some versions of this product contains code from the "
                     "<a href=\"https://www.openssl.org/\">OpenSSL toolkit</a>."
                     "</p>",
                     loadResource(":/LICENSE.OPENSSL"), this));

    m_licenses.append(new LicenseInfo("Boost Software License 1.0",
                     "<p>"
                     "This product contains code from the "
                     "<a href=\"https://github.com/arximboldi/immer\">immer</a>"
                     " project."
                     "</p>",
                     loadResource(":/LICENSE.BOOST"), this));

    if constexpr (CURRENT_PLATFORM == PlatformEnum::Windows) {
        m_licenses.append(new LicenseInfo("GNU Lesser General Public License 2.1",
                         "<p>"
                         "Some versions of this product contains code from the "
                         "following LGPLed libraries: "
                         "<a href=\"https://github.com/jrfonseca/drmingw\">DrMingW</a>"
                         "</p>",
                         loadResource(":/LICENSE.LGPL"), this));
    }
}

QString AboutDialogBackend::aboutText() const
{
    return "<p align=\"center\">"
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
}

QString AboutDialogBackend::authorsText() const
{
    const auto loadResource = [](const QString &path) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return QString("Unable to open resource '%1'.").arg(path);
        } else {
            QTextStream ts(&f);
            return ts.readAll();
        }
    };

    const QStringList authors = loadResource(":/AUTHORS.txt").split("\n");
    if (authors.isEmpty()) {
        return "Could not load authors file.";
    } else {
        QString authorsHtml
            = "<p>The MMapper project is maintained by the following contributors:</p><ul>";
        for (const auto &line : authors) {
            authorsHtml += "<li>" + line + "</li></br>";
        }
        authorsHtml += "</ul>";
        return authorsHtml;
    }
}

QQmlListProperty<LicenseInfo> AboutDialogBackend::licenses()
{
    return QQmlListProperty<LicenseInfo>(this, m_licenses);
}
