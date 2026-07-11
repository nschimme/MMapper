#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors
// Author: Mike Repass <mike.repass@gmail.com> (Taryn)

#include "../global/macros.h"
#include "AdventureLogModel.h"
#include "adventuretracker.h"

#include <memory>

#include <QString>
#include <QWidget>
#include <QtCore>
#include <QtWidgets>

class NODISCARD_QOBJECT AdventureWidget : public QWidget
{
    Q_OBJECT

private:
    AdventureTracker &m_adventureTracker;

    QTextEdit *m_textEdit = nullptr;
    std::unique_ptr<QTextCursor> m_textCursor;
    QAction *m_clearContentAction = nullptr;

public:
    // Message constants live on AdventureLogModel (the QML-facing equivalent
    // of this widget) so both share a single definition.
    static constexpr const int MAX_LINES = AdventureLogModel::MAX_LINES;
    static constexpr const auto DEFAULT_MSG = AdventureLogModel::DEFAULT_MSG;

    static constexpr const auto ACCOMPLISH_MSG = AdventureLogModel::ACCOMPLISH_MSG;
    static constexpr const auto ACHIEVE_MSG = AdventureLogModel::ACHIEVE_MSG;
    static constexpr const auto ACHIEVE_MSG_XP = AdventureLogModel::ACHIEVE_MSG_XP;
    static constexpr const auto DIED_MSG = AdventureLogModel::DIED_MSG;
    static constexpr const auto GAINED_LEVEL_MSG = AdventureLogModel::GAINED_LEVEL_MSG;
    static constexpr const auto HINT_MSG = AdventureLogModel::HINT_MSG;
    static constexpr const auto KILL_TROPHY_MSG = AdventureLogModel::KILL_TROPHY_MSG;

    explicit AdventureWidget(AdventureTracker &at, QWidget *parent);

private:
    void addDefaultContent();
    void addAdventureUpdate(const QString &msg);

public slots:
    void slot_onAccomplishedTask(double xpGained);
    void slot_onAchievedSomething(const QString &achievement, double xpGained);
    void slot_onDied(double xpLost);
    void slot_onGainedLevel();
    void slot_onKilledMob(const QString &mobName, double xpGained);
    void slot_onReceivedHint(const QString &hint);

private slots:
    void slot_contextMenuRequested(const QPoint &pos);
    void slot_actionClearContent(bool checked = false);
};
