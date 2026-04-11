// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "TimerModel.h"

#include "../global/TextUtils.h"
#include "CTimers.h"

#include <QColor>
#include <QDateTime>

namespace {
QString formatMs(int64_t ms)
{
    bool negative = ms < 0;
    if (negative)
        ms = -ms;
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
} // namespace

TimerModel::TimerModel(CTimers &timers, QObject *parent)
    : QAbstractTableModel(parent)
    , m_timers(timers)
{
    m_refreshTimer.setSingleShot(true);
    connect(&m_timers, &CTimers::sig_timerAdded, this, &TimerModel::updateTimerList);
    connect(&m_timers, &CTimers::sig_timerRemoved, this, &TimerModel::updateTimerList);
    connect(&m_timers, &CTimers::sig_timersUpdated, this, &TimerModel::updateTimerList);

    connect(&m_refreshTimer, &QTimer::timeout, this, [this]() {
        emit dataChanged(index(0, ColName),
                         index(static_cast<int>(m_allTimers.size()) - 1, ColTime),
                         {Qt::DisplayRole, ProgressRole});
        if (!m_allTimers.empty()) {
            startRefreshTimer();
        }
    });

    updateTimerList();
}

int TimerModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return static_cast<int>(m_allTimers.size());
}

int TimerModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return ColCount;
}

QVariant TimerModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0
        || static_cast<size_t>(index.row()) >= m_allTimers.size()) {
        return QVariant();
    }

    const TTimer *timer = m_allTimers[static_cast<size_t>(index.row())];

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
                if (timer->isExpired())
                    return "Finished";
                return QDateTime::currentDateTime()
                    .addMSecs(timer->remainingMs())
                    .toString("HH:mm:ss");
            }
            return "-";
        }
    } else if (role == Qt::ForegroundRole) {
        if (timer->isExpired()) {
            return QColor(Qt::red);
        }
    } else if (role == Qt::TextAlignmentRole) {
        if (index.column() == ColTime || index.column() == ColEndTime) {
            return QVariant(static_cast<int>(Qt::AlignCenter));
        }
    } else if (role == ProgressRole) {
        if (timer->isCountdown() && timer->durationMs() > 0) {
            return static_cast<double>(timer->remainingMs())
                   / static_cast<double>(timer->durationMs());
        }
        return QVariant();
    }

    return QVariant();
}

QVariant TimerModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
        case ColName:
            return tr("Name");
        case ColDescription:
            return tr("Description");
        case ColTime:
            return tr("Time");
        case ColEndTime:
            return tr("End Time");
        }
    }
    return QVariant();
}

const TTimer *TimerModel::timerAt(int row) const
{
    if (row < 0 || static_cast<size_t>(row) >= m_allTimers.size())
        return nullptr;
    return m_allTimers[static_cast<size_t>(row)];
}

bool TimerModel::hasAnyDescriptions() const
{
    for (const auto *timer : m_allTimers) {
        if (!timer->getDescription().empty())
            return true;
    }
    return false;
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

    if (m_allTimers.empty()) {
        m_refreshTimer.stop();
    } else if (!m_refreshTimer.isActive()) {
        startRefreshTimer();
    }
}

void TimerModel::startRefreshTimer()
{
    const auto now = QDateTime::currentDateTime();
    const int delay = 1000 - static_cast<int>(now.time().msec());
    m_refreshTimer.start(delay > 0 ? delay : 1000);
}
