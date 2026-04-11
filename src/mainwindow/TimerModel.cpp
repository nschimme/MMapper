// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "TimerModel.h"
#include "../timers/CTimers.h"
#include "../global/TextUtils.h"
#include <QDateTime>
#include <QColor>

namespace {
QString formatMs(int64_t ms) {
    bool negative = ms < 0;
    if (negative) ms = -ms;
    int64_t totalSecs = ms / 1000;
    int64_t h = totalSecs / 3600;
    int64_t m = (totalSecs / 60) % 60;
    int64_t s = totalSecs % 60;

    QString res;
    if (h > 0) {
        res += QString::number(h) + ":";
    }
    res += QString("%1:%2").arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
    return negative ? "-" + res : res;
}
}

TimerModel::TimerModel(CTimers &timers, QObject *parent)
    : QAbstractTableModel(parent)
    , m_timers(timers)
{
    connect(&m_timers, &CTimers::sig_timerAdded, this, &TimerModel::updateTimerList);
    connect(&m_timers, &CTimers::sig_timerRemoved, this, &TimerModel::updateTimerList);
    connect(&m_timers, &CTimers::sig_timersUpdated, this, &TimerModel::updateTimerList);

    connect(&m_refreshTimer, &QTimer::timeout, this, [this]() {
        if (m_allTimers.empty()) return;
        emit dataChanged(index(0, ColTime), index(m_allTimers.size() - 1, ColTime), {Qt::DisplayRole});
    });
    m_refreshTimer.start(1000);

    updateTimerList();
}

int TimerModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return static_cast<int>(m_allTimers.size());
}

int TimerModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return ColCount;
}

QVariant TimerModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= static_cast<int>(m_allTimers.size())) {
        return QVariant();
    }

    const TTimer *timer = m_allTimers[index.row()];

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case ColName:
            return mmqt::toQStringUtf8(timer->getName());
        case ColDescription:
            return mmqt::toQStringUtf8(timer->getDescription());
        case ColTime:
            if (timer->isCountdown()) {
                return formatMs(timer->remainingMs());
            } else {
                return formatMs(timer->elapsedMs());
            }
        case ColEndTime:
            if (timer->isCountdown()) {
                if (timer->isExpired()) return "Finished";
                return QDateTime::currentDateTime().addMSecs(timer->remainingMs()).toString("HH:mm:ss");
            }
            return "-";
        }
    } else if (role == Qt::ForegroundRole) {
        if (timer->isExpired()) {
            return QColor(Qt::red);
        }
    } else if (role == Qt::TextAlignmentRole) {
        if (index.column() == ColTime || index.column() == ColEndTime) {
            return Qt::AlignCenter;
        }
    }

    return QVariant();
}

QVariant TimerModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
        case ColName: return tr("Name");
        case ColDescription: return tr("Description");
        case ColTime: return tr("Time");
        case ColEndTime: return tr("End Time");
        }
    }
    return QVariant();
}

const TTimer* TimerModel::timerAt(int row) const
{
    if (row < 0 || row >= static_cast<int>(m_allTimers.size())) return nullptr;
    return m_allTimers[row];
}

void TimerModel::updateTimerList()
{
    beginResetModel();
    m_allTimers.clear();
    for (const auto &t : m_timers.countdowns()) {
        m_allTimers.push_back(&t);
    }
    for (const auto &t : m_timers.timers()) {
        m_allTimers.push_back(&t);
    }
    endResetModel();
}
