#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "../global/macros.h"
#include <QObject>
#include <QString>

class NODISCARD_QOBJECT UpdateViewModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
    Q_PROPERTY(bool upgradeButtonEnabled READ upgradeButtonEnabled NOTIFY statusChanged)

public:
    explicit UpdateViewModel(QObject *parent = nullptr);

    NODISCARD QString statusText() const { return m_statusText; }
    NODISCARD bool upgradeButtonEnabled() const { return m_upgradeButtonEnabled; }

    void checkUpdates();

signals:
    void statusChanged();
    void sig_showUpdateDialog();

private:
    QString m_statusText;
    bool m_upgradeButtonEnabled = false;
};
