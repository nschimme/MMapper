#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "../global/macros.h"
#include <QObject>
#include <QStringList>

class AdventureTracker;

class NODISCARD_QOBJECT AdventureViewModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList messages READ messages NOTIFY messagesChanged)

public:
    explicit AdventureViewModel(AdventureTracker &tracker, QObject *parent = nullptr);

    NODISCARD QStringList messages() const { return m_messages; }

    void clear();

signals:
    void messagesChanged();
    void sig_messageAdded(const QString &msg);

private slots:
    void onAccomplishedTask(double xpGained);
    void onAchievedSomething(const QString &achievement, double xpGained);
    void onDied(double xpLost);
    void onGainedLevel();
    void onKilledMob(const QString &mobName, double xpGained);
    void onReceivedHint(const QString &hint);

private:
    void addMessage(const QString &msg);
    AdventureTracker &m_tracker;
    QStringList m_messages;
};
