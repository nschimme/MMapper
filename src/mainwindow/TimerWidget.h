#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include <QWidget>
#include <QTableView>

class CTimers;
class TimerModel;
class QMenu;

class TimerWidget final : public QWidget
{
    Q_OBJECT

private:
    CTimers &m_timers;
    TimerModel *m_model = nullptr;
    QTableView *m_view = nullptr;

public:
    explicit TimerWidget(CTimers &timers, QWidget *parent = nullptr);

private slots:
    void showContextMenu(const QPoint &pos);
    void clearExpired();
};
