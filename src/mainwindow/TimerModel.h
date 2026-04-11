#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include <vector>

#include <QAbstractTableModel>
#include <QTimer>

class CTimers;
class TTimer;

class TimerModel final : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column { ColName = 0, ColDescription, ColTime, ColEndTime, ColCount };
    enum Role { ProgressRole = Qt::UserRole + 1 };

private:
    CTimers &m_timers;
    std::vector<const TTimer *> m_allTimers;
    QTimer m_refreshTimer;

public:
    explicit TimerModel(CTimers &timers, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section,
                        Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    const TTimer *timerAt(int row) const;

private slots:
    void updateTimerList();
};
