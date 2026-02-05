#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "../global/macros.h"
#include <QObject>
#include <QString>

class NODISCARD_QOBJECT AutoLogPageViewModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString autoLogDirectory READ autoLogDirectory WRITE setAutoLogDirectory NOTIFY settingsChanged)
    Q_PROPERTY(bool autoLog READ autoLog WRITE setAutoLog NOTIFY settingsChanged)
    Q_PROPERTY(int cleanupStrategy READ cleanupStrategy WRITE setCleanupStrategy NOTIFY settingsChanged)
    Q_PROPERTY(int deleteWhenLogsReachDays READ deleteWhenLogsReachDays WRITE setDeleteWhenLogsReachDays NOTIFY settingsChanged)
    Q_PROPERTY(int deleteWhenLogsReachBytes READ deleteWhenLogsReachBytes WRITE setDeleteWhenLogsReachBytes NOTIFY settingsChanged)
    Q_PROPERTY(bool askDelete READ askDelete WRITE setAskDelete NOTIFY settingsChanged)
    Q_PROPERTY(int rotateWhenLogsReachBytes READ rotateWhenLogsReachBytes WRITE setRotateWhenLogsReachBytes NOTIFY settingsChanged)

public:
    explicit AutoLogPageViewModel(QObject *parent = nullptr);

    NODISCARD QString autoLogDirectory() const; void setAutoLogDirectory(const QString &v);
    NODISCARD bool autoLog() const; void setAutoLog(bool v);
    NODISCARD int cleanupStrategy() const; void setCleanupStrategy(int v);
    NODISCARD int deleteWhenLogsReachDays() const; void setDeleteWhenLogsReachDays(int v);
    NODISCARD int deleteWhenLogsReachBytes() const; void setDeleteWhenLogsReachBytes(int v);
    NODISCARD bool askDelete() const; void setAskDelete(bool v);
    NODISCARD int rotateWhenLogsReachBytes() const; void setRotateWhenLogsReachBytes(int v);

    void loadConfig();

signals:
    void settingsChanged();
};
