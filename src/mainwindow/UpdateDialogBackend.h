#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "../global/Array.h"

#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QDebug>

class NODISCARD CompareVersion final
{
private:
    MMapper::Array<int, 3> m_parts;

public:
    explicit CompareVersion(const QString &versionStr) noexcept;

public:
    NODISCARD bool operator>(const CompareVersion &other) const;
    NODISCARD bool operator==(const CompareVersion &other) const;

    NODISCARD int major() const { return m_parts[0]; }
    NODISCARD int minor() const { return m_parts[1]; }
    NODISCARD int patch() const { return m_parts[2]; }

public:
    NODISCARD QString toQString() const;
    explicit operator QString() const { return toQString(); }
    friend QDebug operator<<(QDebug os, const CompareVersion &compareVersion)
    {
        return os << compareVersion.toQString();
    }
};

class UpdateDialogBackend : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString downloadUrl READ downloadUrl)

public:
    explicit UpdateDialogBackend(QObject *parent = nullptr);

    Q_INVOKABLE void checkForUpdate(bool interactive);
    QString downloadUrl() const { return m_downloadUrl; }

signals:
    void updateStatus(const QString &message, bool enableUpgradeButton, bool showAndUpdateDialog);

private slots:
    void managerFinished(QNetworkReply *reply);

private:
    QString findDownloadUrlForRelease(const QJsonObject &releaseObject) const;

    QNetworkAccessManager m_manager;
    QString m_downloadUrl;
    bool m_interactive = false;
};
