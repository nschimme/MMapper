// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Kalev Lember <kalev@smartlink.ee> (Kalev)

#include "aboutdialog.h"

#include "../global/ConfigConsts-Computed.h"
#include "../global/ConfigEnums.h"
#include "../global/Version.h"

#include <QString>
#include <QtConfig>
#include <QtGui>
#include <QtWidgets>

namespace {
struct LicenseInfo {
    QString title;
    QString introText;
    QString resourcePath;
};
} // namespace

NODISCARD static QString getBuildInformation()
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
        .arg(QString::fromUtf8(getMMapperBranch()))
        .arg(get_compiler());
}

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowIcon(QIcon(":/icons/m.png"));
    setupUi(this);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    /* About tab */
    pixmapLabel->setPixmap(QPixmap(":/pixmaps/splash.png"));
    const auto about_text = []() -> QString {
        return "<p align=\"center\">"
               "<h3>"
               "<u>"
               + tr("MMapper %1").arg(QString::fromUtf8(getMMapperVersion()))
               + "</h3>"
                 "</u>"
                 "</p>"
                 "<p align=\"center\">"
               + getBuildInformation()
               + tr("Based on Qt %1 (%2 bit)")
                     .arg(qVersion())
                     .arg(static_cast<size_t>(QSysInfo::WordSize))
               + "</p>";
    };
    aboutText->setText(about_text());

    const auto loadResource = [](const QString &path) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return QString("Unable to open resource '%1'.").arg(path);
        } else {
            QTextStream ts(&f);
            return ts.readAll();
        }
    };

    /* Authors tab */
    authorsView->clear();
    const QStringList authors = loadResource(":/AUTHORS.txt").split("\n");
    if (authors.isEmpty()) {
        authorsView->setHtml("Could not load authors file.");
    } else {
        QString authorsHtml
            = "<p>The MMapper project is maintained by the following contributors:</p><ul>";
        for (const auto &line : authors) {
            authorsHtml += "<li>" + line + "</li></br>";
        }
        authorsHtml += "</ul>";
        authorsView->setHtml(authorsHtml);
    }

    /* Licenses tab */
    QList<LicenseInfo> licenses;
    licenses.append({tr("GNU General Public License 2.0"),
                     "<p>"
                     "This program is free software; you can redistribute it and/or "
                     "modify it under the terms of the GNU General Public License "
                     "as published by the Free Software Foundation; either version 2 "
                     "of the License, or (at your option) any later version."
                     "</p><p>"
                     "This program is distributed in the hope that it will be useful, "
                     "but WITHOUT ANY WARRANTY; without even the implied warranty of "
                     "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE."
                     "</p><p>"
                     "See the GNU General Public License for more details. "
                     "</p>",
                     ":/LICENSE.GPL2"});

    licenses.append({tr("DejaVu Fonts License"),
                     "<p>" +
                         tr("This license applies to the file "
                            "<code>src/resources/fonts/DejaVuSansMono.ttf</code>") +
                         "</p>",
                     ":/fonts/LICENSE"});

    licenses.append({tr("GLM License"),
                     "<p>" +
                         tr("This product contains code from the "
                            "<a href=\"https://glm.g-truc.net/\">OpenGL Mathematics (GLM)</a>"
                            " project.") +
                         "</p>",
                     ":/LICENSE.GLM"});

    licenses.append({tr("QtKeychain License"),
                     "<p>" +
                         tr("This product contains code from the "
                            "<a href=\"https://github.com/frankosterfeld/qtkeychain\">QtKeychain</a>"
                            " project.") +
                         "</p>",
                     ":/LICENSE.QTKEYCHAIN"});

    licenses.append({tr("OpenSSL License"),
                     "<p>" +
                         tr("Some versions of this product contains code from the "
                            "<a href=\"https://www.openssl.org/\">OpenSSL toolkit</a>.") +
                         "</p>",
                     ":/LICENSE.OPENSSL"});

    licenses.append({tr("Boost Software License 1.0"),
                     "<p>" +
                         tr("This product contains code from the "
                            "<a href=\"https://github.com/arximboldi/immer\">immer</a>"
                            " project.") +
                         "</p>",
                     ":/LICENSE.BOOST"});

    if constexpr (CURRENT_PLATFORM == PlatformEnum::Windows) {
        licenses.append({tr("GNU Lesser General Public License 2.1"),
                         "<p>" +
                             tr("Some versions of this product contains code from the "
                                "following LGPLed libraries: "
                                "<a href=\"https://github.com/jrfonseca/drmingw\">DrMingW</a>") +
                             "</p>",
                         ":/LICENSE.LGPL"});
    }

    QString allLicensesTextHtml;
    if (!licenses.isEmpty()) {
        const auto &firstLicense = licenses.first();
        allLicensesTextHtml += "<h1>" + firstLicense.title + "</h1>";
        if (!firstLicense.introText.isEmpty()) {
            allLicensesTextHtml += firstLicense.introText;
        }
        allLicensesTextHtml += "<pre>" + loadResource(firstLicense.resourcePath) + "</pre>";

        for (int i = 1; i < licenses.size(); ++i) {
            const auto &license = licenses.at(i);
            allLicensesTextHtml += "<hr/><h1>" + license.title + "</h1>";
            if (!license.introText.isEmpty()) {
                allLicensesTextHtml += license.introText;
            }
            allLicensesTextHtml += "<pre>" + loadResource(license.resourcePath) + "</pre>";
        }
    }

    licenseView->setText(allLicensesTextHtml);
    setFixedFont(licenseView);

    adjustSize();
}

void AboutDialog::setFixedFont(QTextBrowser *browser)
{
    QFont fixed;
    fixed.setStyleHint(QFont::TypeWriter);
    fixed.setFamily("Courier");
    browser->setFont(fixed);
}
