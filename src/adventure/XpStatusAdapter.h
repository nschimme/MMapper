#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/ChangeMonitor.h"
#include "../global/macros.h"
#include "adventuretracker.h"

#include <memory>

#include <QObject>
#include <QString>

// QML-facing replacement for XPStatusWidget. Ports its updateContent()/
// enterEvent()/leaveEvent()/clicked() logic; MainWindow wires the signals
// below to the actual QStatusBar and Adventure dock (this class has no
// QWidget dependency of its own, matching the always-compiled mmapper_SRCS
// pattern used by DescriptionAdapter/QmlConfig).
class NODISCARD_QOBJECT XpStatusAdapter final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString text READ getText NOTIFY sig_changed)
    // Named "shown" (not "visible") so it can be bound directly to the QML
    // root item's `visible` property without name-clashing with Item.visible.
    Q_PROPERTY(bool shown READ isShown NOTIFY sig_changed)

private:
    AdventureTracker &m_tracker;
    std::shared_ptr<AdventureSession> m_session;
    QString m_text;
    bool m_shown = false;
    Signal2Lifetime m_lifetime;

public:
    explicit XpStatusAdapter(AdventureTracker &at, QObject *parent);

public:
    NODISCARD QString getText() const { return m_text; }
    NODISCARD bool isShown() const { return m_shown; }

public:
    // QML calls these from HoverHandler/TapHandler; they replicate
    // XPStatusWidget::enterEvent()/leaveEvent()/the clicked() lambda wired up
    // in MainWindow::setupStatusBar().
    Q_INVOKABLE void hoverEntered();
    Q_INVOKABLE void hoverExited();
    Q_INVOKABLE void clicked();

signals:
    void sig_changed();
    // Mirrors XPStatusWidget::enterEvent()'s m_statusBar->showMessage(msg)
    // call (no timeout: the message persists until sig_clearStatusMessage()).
    void sig_showStatusMessage(const QString &message);
    // Mirrors XPStatusWidget::leaveEvent()'s m_statusBar->clearMessage() call.
    void sig_clearStatusMessage();
    // Mirrors the toggle-the-Adventure-dock lambda MainWindow connects to
    // XPStatusWidget's clicked() signal.
    void sig_toggleAdventurePanel();

private slots:
    void slot_updatedSession(const std::shared_ptr<AdventureSession> &session);

private:
    void updateContent();
};
