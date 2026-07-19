#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"

#include <QObject>

// Owns the visibility state of Shell B's 8 side-panel docks (see
// QmlShellWindow.cpp/MainShell.qml), analogous to the QDockWidget::hide()/
// show() and toggleViewAction() calls MainWindow makes directly on each
// QmlDockWidget in mainwindow.cpp. Under the single QQmlApplicationEngine
// the QML shell uses (unlike each QmlDockWidget's own private QQuickWidget
// engine), the docks are plain QQC2.SplitView children rather than
// QDockWidgets, so something has to own their visible state instead --
// that's this class. Exposed to QML as a single "dockLayout" context
// property (see QmlShellWindow.cpp); DockPanel.qml's close button and
// MainShell.qml's "Side Panels" menu items both bind against these
// properties.
//
// A flat list of 8 named bool properties (rather than a
// QVariantMap/id-keyed API) was chosen because it lets QML bind each
// dock's `visible` and each menu item's `checked` directly as a property
// expression (`dockLayout.logVisible`) instead of going through
// `dockLayout.isVisible("log")`-style invokables, which is both more
// verbose in the 8 call sites that need it and loses QML's automatic
// re-evaluation on change (Q_PROPERTY's NOTIFY signal drives that; a plain
// Q_INVOKABLE getter would not update automatically).
//
// Deliberately NOT implemented here (left for a later "shell lifecycle"
// commit, see the task report): persisting dock area/index/size/visibility
// to disk (mirrors MainWindow::writeSettings()'s saveState()/restoreState()
// pair) and floating docks (mirrors QDockWidget::DockWidgetFloatable).
// MainShell.qml's dock areas/order/sizes are presently hard-coded to the
// same defaults mainwindow.cpp's addDockWidget() calls establish.
class NODISCARD_QOBJECT DockLayoutController final : public QObject
{
    Q_OBJECT

    // Defaults mirror mainwindow.cpp's dock construction: Log/Room/
    // Adventure/Timers/Tasks start hidden (dock->hide()); Group/Description/
    // Client start visible (no hide() call).
    Q_PROPERTY(bool logVisible MEMBER m_logVisible NOTIFY logVisibleChanged)
    Q_PROPERTY(bool groupVisible MEMBER m_groupVisible NOTIFY groupVisibleChanged)
    Q_PROPERTY(bool roomVisible MEMBER m_roomVisible NOTIFY roomVisibleChanged)
    Q_PROPERTY(bool adventureVisible MEMBER m_adventureVisible NOTIFY adventureVisibleChanged)
    Q_PROPERTY(bool descriptionVisible MEMBER m_descriptionVisible NOTIFY descriptionVisibleChanged)
    Q_PROPERTY(bool timersVisible MEMBER m_timersVisible NOTIFY timersVisibleChanged)
    Q_PROPERTY(bool tasksVisible MEMBER m_tasksVisible NOTIFY tasksVisibleChanged)
    Q_PROPERTY(bool clientVisible MEMBER m_clientVisible NOTIFY clientVisibleChanged)

private:
    bool m_logVisible = false;
    bool m_groupVisible = true;
    bool m_roomVisible = false;
    bool m_adventureVisible = false;
    bool m_descriptionVisible = true;
    bool m_timersVisible = false;
    bool m_tasksVisible = false;
    bool m_clientVisible = true;

public:
    explicit DockLayoutController(QObject *parent = nullptr);

signals:
    void logVisibleChanged();
    void groupVisibleChanged();
    void roomVisibleChanged();
    void adventureVisibleChanged();
    void descriptionVisibleChanged();
    void timersVisibleChanged();
    void tasksVisibleChanged();
    void clientVisibleChanged();
};
