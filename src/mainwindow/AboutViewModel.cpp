// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "AboutViewModel.h"
#include "../global/Version.h"
#include <QFile>
#include <QTextStream>
#include <QSysInfo>

AboutViewModel::AboutViewModel(QObject *p) : QObject(p) {}

QString AboutViewModel::aboutHtml() const {
    return QString("<p align=\"center\"><h3><u>MMapper %1</u></h3></p>"
                   "<p align=\"center\">Built on branch %2<br>"
                   "Based on Qt %3 (%4 bit)</p>")
        .arg(QString::fromUtf8(getMMapperVersion()))
        .arg(QString::fromUtf8(getMMapperBranch()))
        .arg(qVersion())
        .arg(static_cast<size_t>(QSysInfo::WordSize));
}

QString AboutViewModel::authorsHtml() const {
    QFile f(":/AUTHORS.txt");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return "Could not load authors file.";
    QTextStream ts(&f);
    QStringList authors = ts.readAll().split("\n");
    QString html = "<p>The MMapper project is maintained by the following contributors:</p><ul>";
    for (const auto &line : authors) {
        if (!line.trimmed().isEmpty()) html += "<li>" + line + "</li>";
    }
    html += "</ul>";
    return html;
}
