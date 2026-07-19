#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/RuleOf5.h"
#include "../global/macros.h"

#include <QObject>
#include <QString>

// QML-facing progress-popup state for Shell B's async load/merge/save tasks
// (see QmlShellWindow.cpp's loadFile()/mergeFile()/saveMapFile() and
// MainShell.qml's ioProgressPopup, which binds directly against an instance
// of this class exposed as the "ioTask" context property). Mirrors
// MainWindow::AsyncIO's QProgressDialog (mainwindow-async.cpp's
// createNewProgressDialog()/updateStatus()): a label fed from the running
// task's ProgressCounter::Status::msg, a 0-99 percent fed from
// Status::percent() (clamped to 99 the same way AsyncIO::updateStatus()
// does -- 100% is reserved for "actually finished", which this popup
// represents by disappearing instead), and cancelable only when the task's
// AllowCancelEnum is Allow (loads/merges; saves forbid cancel, matching
// AsyncIO::beginAsyncIO()'s "Save forbids cancel" rule).
//
// Deliberately a small hand-rolled QObject rather than a generic model over
// the whole async_tasks registry (unlike TasksModel.h, which lists every
// task): QmlShellWindow only ever starts one IO task at a time itself (see
// its m_ioInProgress flag), so there is exactly one task to show, and a
// flat set of properties is simpler for MainShell.qml's popup to bind
// against than a 1-row list model would be.
class NODISCARD_QOBJECT IoTaskController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ isActive NOTIFY sig_changed)
    Q_PROPERTY(QString label READ getLabel NOTIFY sig_changed)
    Q_PROPERTY(int percent READ getPercent NOTIFY sig_changed)
    Q_PROPERTY(bool cancelable READ isCancelable NOTIFY sig_changed)

private:
    bool m_active = false;
    QString m_label;
    int m_percent = 0;
    bool m_cancelable = false;

public:
    explicit IoTaskController(QObject *parent = nullptr);
    ~IoTaskController() final;
    DELETE_CTORS_AND_ASSIGN_OPS(IoTaskController);

public:
    NODISCARD bool isActive() const { return m_active; }
    NODISCARD QString getLabel() const { return m_label; }
    NODISCARD int getPercent() const { return m_percent; }
    NODISCARD bool isCancelable() const { return m_cancelable; }

public:
    // Called by QmlShellWindow when it starts/polls/finishes an
    // async_tasks::IO task (see beginIoTaskProgress()/tickIoTaskProgress()/
    // endIoTaskProgress() in QmlShellWindow.cpp).
    void begin(const QString &label, bool cancelable);
    void update(const QString &label, int percent);
    void end();

public:
    // Bound to the popup's Cancel button (see MainShell.qml); no-op if the
    // current task forbids cancel (mirrors AsyncIO::request_cancel() only
    // being reachable via a QProgressDialog whose cancel button
    // createNewProgressDialog() left disabled for non-cancelable tasks --
    // this is the QML-side equivalent of that same guard).
    Q_INVOKABLE void cancel();

signals:
    void sig_changed();
    // QmlShellWindow connects this to the running task's
    // ProgressCounter::requestCancel() (via the AsyncTaskHandle it holds).
    void sig_cancelRequested();
};
