#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "../global/CompareVersion.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QNetworkAccessManager>

class NODISCARD_QOBJECT UpdateDialog : public QDialog
{
    Q_OBJECT

private:
    QNetworkAccessManager m_manager;
    QString m_downloadUrl;
    QLabel *m_text = nullptr;
    QDialogButtonBox *m_buttonBox = nullptr;

public:
    explicit UpdateDialog(QWidget *parent);

    void open() override;

private slots:
    void managerFinished(QNetworkReply *reply);

private:
    void setUpdateStatus(const QString &message, bool enableUpgradeButton, bool showAndUpdateDialog);
    QString findDownloadUrlForRelease(const QJsonObject &releaseObject) const;
};
