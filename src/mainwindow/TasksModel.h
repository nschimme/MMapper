#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/AsyncTasks.h"
#include "../global/macros.h"

#include <utility>
#include <vector>

#include <QAbstractListModel>
#include <QTimer>

// QML-facing model that mirrors TasksPanel (the QWidget-based Tasks dock):
// it polls the async_tasks registry every 250ms on the same cadence as the
// widget's own QTimer, diffing against the previously-known rows so the
// QML ListView only sees insert/remove/update deltas instead of a full
// reset every tick.
//
// holdRemovals mirrors the widget's underMouse() removal suppression
// (TasksPanel::refresh_data(), "allowRemoval"): while true, tasks that have
// disappeared from the registry are left in the model (still refreshed in
// place, showing their final Finished/Canceled status) instead of being
// removed, so a user hovering the list doesn't have rows yanked out from
// under the pointer. TasksPanel.qml binds this to a HoverHandler over the
// ListView.
class NODISCARD_QOBJECT TasksModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(
        bool holdRemovals READ getHoldRemovals WRITE setHoldRemovals NOTIFY holdRemovalsChanged)

public:
    enum RoleEnum {
        TaskIdRole = Qt::UserRole + 1,
        StatusTextRole,
        PercentRole,
        CanCancelRole,
    };

private:
    struct NODISCARD Row final
    {
        async_tasks::AsyncTaskHandle task;
        QString statusText;
        int percent = 0;
        bool canCancel = false;

        explicit Row(async_tasks::AsyncTaskHandle moved_task)
            : task(std::move(moved_task))
        {}
    };

private:
    std::vector<Row> m_rows;
    QTimer m_refreshTimer;
    bool m_holdRemovals = false;

public:
    explicit TasksModel(QObject *parent = nullptr);

    NODISCARD int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    NODISCARD QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    NODISCARD QHash<int, QByteArray> roleNames() const override;

    NODISCARD bool getHoldRemovals() const { return m_holdRemovals; }
    void setHoldRemovals(bool hold);

    Q_INVOKABLE void cancelTask(int row);

signals:
    void holdRemovalsChanged();

private:
    void refresh();
    NODISCARD static QString buildStatusText(const async_tasks::AsyncTaskHandle &task);
    NODISCARD static int computePercent(const async_tasks::AsyncTaskHandle &task);
};
