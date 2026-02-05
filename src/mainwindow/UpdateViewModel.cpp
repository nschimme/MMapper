// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "UpdateViewModel.h"
#include "../configuration/configuration.h"
#include "../global/ConfigConsts-Computed.h"
#include "../global/Version.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <QSysInfo>

namespace {
NODISCARD const char *getArchitectureRegexPattern() {
    if (QSysInfo::currentCpuArchitecture() == "x86_64") return "(x86_64|amd64|x64)";
    if (QSysInfo::currentCpuArchitecture() == "i386") return "(i386|x86(?!_64))";
    if (QSysInfo::currentCpuArchitecture() == "arm64") return "(arm64|aarch64)";
    if (QSysInfo::currentCpuArchitecture() == "arm") return "(arm(?!64)|armhf)";
    return "";
}
}

UpdateViewModel::UpdateViewModel(QObject *p) : QObject(p) {}

void UpdateViewModel::checkUpdates() {
    m_statusText = "Checking for updates...";
    emit statusChanged();

    QNetworkAccessManager *mgr = new QNetworkAccessManager(this);
    auto url = isMMapperBeta() ? "https://api.github.com/repos/mume/mmapper/git/ref/tags/beta" : "https://api.github.com/repos/mume/mmapper/releases/latest";
    QNetworkRequest req((QUrl(url)));
    req.setHeader(QNetworkRequest::ServerHeader, "application/json");

    connect(mgr, &QNetworkAccessManager::finished, this, [this, mgr](QNetworkReply *reply) {
        reply->deleteLater();
        mgr->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            m_statusText = "Update check failed: " + reply->errorString();
            emit statusChanged();
            return;
        }

        QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        m_statusText = "A new version is available!";
        m_upgradeButtonEnabled = true;
        emit statusChanged();
        emit sig_showUpdateDialog();
    });
    mgr->get(req);
}
