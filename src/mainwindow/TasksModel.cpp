// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "TasksModel.h"

#include "../global/AnsiOstream.h"
#include "../global/AnsiTextUtils.h"
#include "../global/TextUtils.h"

#include <functional>
#include <map>
#include <sstream>

namespace {
constexpr int TASKS_REFRESH_INTERVAL_MS = 250;
} // namespace

TasksModel::TasksModel(QObject *const parent)
    : QAbstractListModel(parent)
{
    connect(&m_refreshTimer, &QTimer::timeout, this, &TasksModel::refresh);
    m_refreshTimer.start(TASKS_REFRESH_INTERVAL_MS);
}

int TasksModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_rows.size());
}

QVariant TasksModel::data(const QModelIndex &index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || static_cast<size_t>(index.row()) >= m_rows.size()) {
        return QVariant();
    }

    const Row &row = m_rows[static_cast<size_t>(index.row())];
    switch (role) {
    case TaskIdRole:
        return static_cast<qulonglong>(row.task.getId());
    case StatusTextRole:
        return row.statusText;
    case PercentRole:
        return row.percent;
    case CanCancelRole:
        return row.canCancel;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> TasksModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[TaskIdRole] = "taskId";
    roles[StatusTextRole] = "statusText";
    roles[PercentRole] = "percent";
    roles[CanCancelRole] = "canCancel";
    return roles;
}

void TasksModel::setHoldRemovals(const bool hold)
{
    if (m_holdRemovals == hold) {
        return;
    }
    const bool wasHolding = m_holdRemovals;
    m_holdRemovals = hold;
    emit holdRemovalsChanged();

    // Transitioning from held to released: catch up immediately so any
    // removals deferred while the user was hovering happen right away,
    // rather than waiting up to TASKS_REFRESH_INTERVAL_MS for the next
    // timer tick. Mirrors TasksPanel::refresh_data(), which recomputes
    // allowRemoval from underMouse() fresh on every tick.
    if (wasHolding && !hold) {
        refresh();
    }
}

void TasksModel::cancelTask(const int row)
{
    if (row < 0 || static_cast<size_t>(row) >= m_rows.size()) {
        return;
    }

    // Mirrors CancelTaskButton::CancelTaskButton() in TasksPanel.cpp: only
    // request cancellation if the task actually allows it, and call
    // requestCancel() directly rather than going through the logged
    // async_tasks::cancel() path used by the widget's context menu (QML
    // unifies both the row button and the context-menu action into this one
    // entry point).
    Row &r = m_rows[static_cast<size_t>(row)];
    if (r.task.getCanCancel() == AllowCancelEnum::Allow) {
        r.task.requestCancel();
    }
}

int TasksModel::computePercent(const async_tasks::AsyncTaskHandle &task)
{
    // Mirrors ListItem::updateProgress() in TasksPanel.cpp.
    if (task.getIsRemoved()) {
        return task.getProgressCounter().hasRequestedCancel() ? 0 : 100;
    }
    return static_cast<int>(task.getStatus().percent());
}

QString TasksModel::buildStatusText(const async_tasks::AsyncTaskHandle &task)
{
    // Verbatim port of ListItem::updateProgress()'s text-building logic in
    // TasksPanel.cpp.
    const auto &pc = task.getProgressCounter();
    const auto status = pc.getStatus();
    const bool is_removed = task.getIsRemoved();
    const auto pct = computePercent(task);

    const auto elapsed = std::invoke([&task]() -> QString {
        std::ostringstream oss;
        {
            AnsiOstream aos{oss};
            async_tasks::formatElapsedSeconds(aos, task.getElapsedTime());
        }
        return mmqt::toQStringUtf8(strip_ansi(oss.str()));
    });

    const auto statusMsg = std::invoke([&status, is_removed, pct, &pc]() -> QString {
        if (is_removed) {
            return pc.hasRequestedCancel() ? "Canceled." : "Finished.";
        }

        auto tmp = status.msg.toQString();
        if (!tmp.isEmpty()) {
            tmp[0] = tmp[0].toUpper();
        }
        return QString("%1... (%2%)").arg(tmp).arg(pct);
    });

    return QString("Task #%1: %2 (%3) [elapsed: %4]\nStatus: %5")
        .arg(task.getId())
        .arg(mmqt::toQStringUtf8(task.getName()))
        .arg((task.getCanCancel() == AllowCancelEnum::Allow) ? "cancelable" : "non-cancelable")
        .arg(elapsed)
        .arg(statusMsg);
}

void TasksModel::refresh()
{
    // Mirrors TasksPanel::refresh_data() in TasksPanel.cpp, replacing the
    // QWidget layout/ListItem bookkeeping with QAbstractListModel row
    // operations. "allowRemoval" here is "!holdRemovals" (the QML
    // equivalent of the widget's "!underMouse()").
    const bool allowRemoval = !m_holdRemovals;

    std::map<size_t, async_tasks::AsyncTaskHandle> currentTasks;
    async_tasks::for_each([&currentTasks](const async_tasks::AsyncTaskHandle &task) {
        currentTasks.emplace(task.getId(), task);
    });

    std::map<size_t, bool> seen;

    // step 1: refresh existing rows, and mark ones to remove.
    std::vector<int> toRemove;
    for (int i = 0; i < static_cast<int>(m_rows.size()); ++i) {
        Row &row = m_rows[static_cast<size_t>(i)];
        const size_t task_id = row.task.getId();

        const bool stillPresent = currentTasks.find(task_id) != currentTasks.end();
        if (stillPresent) {
            seen[task_id] = true;
        }

        if (stillPresent || !allowRemoval) {
            // Refresh in place using the row's own retained handle (kept
            // alive by its shared_ptr even after the registry has dropped
            // its copy), same as the widget does for held-back rows.
            const QString newStatusText = buildStatusText(row.task);
            const int newPercent = computePercent(row.task);
            const bool newCanCancel = row.task.getCanCancel() == AllowCancelEnum::Allow
                                      && !row.task.getProgressCounter().hasRequestedCancel();

            if (newStatusText != row.statusText || newPercent != row.percent
                || newCanCancel != row.canCancel) {
                row.statusText = newStatusText;
                row.percent = newPercent;
                row.canCancel = newCanCancel;
                const QModelIndex idx = index(i);
                emit dataChanged(idx, idx, {});
            }
        }

        if (!stillPresent && allowRemoval) {
            toRemove.push_back(i);
        }
    }

    if (allowRemoval && !toRemove.empty()) {
        // Remove from the back so earlier indices stay valid.
        for (auto it = toRemove.rbegin(); it != toRemove.rend(); ++it) {
            const int i = *it;
            beginRemoveRows(QModelIndex(), i, i);
            m_rows.erase(m_rows.begin() + i);
            endRemoveRows();
        }
    }

    // step 2: add tasks that weren't already in the model.
    for (const auto &[id, task] : currentTasks) {
        if (seen[id]) {
            continue;
        }
        seen[id] = true;

        Row newRow{task};
        newRow.statusText = buildStatusText(task);
        newRow.percent = computePercent(task);
        newRow.canCancel = task.getCanCancel() == AllowCancelEnum::Allow
                           && !task.getProgressCounter().hasRequestedCancel();

        const int newIndex = static_cast<int>(m_rows.size());
        beginInsertRows(QModelIndex(), newIndex, newIndex);
        m_rows.push_back(std::move(newRow));
        endInsertRows();
    }
}
