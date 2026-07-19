#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"

#include <QObject>

// Owns the visibility state of Shell B's 9 toolbars (see
// QmlShellWindow.cpp/MainShell.qml), analogous to the QToolBar::hide()/
// show() and toggleViewAction() calls MainWindow makes directly on each
// QToolBar in mainwindow.cpp's setupToolBars(). Mirrors DockLayoutController
// (see its file comment for the full rationale): a flat list of named bool
// properties instead of a QVariantMap/id-keyed API, one per toolbar, so
// MainShell.qml can bind each toolbar's `visible` and the View->Toolbars
// menu items' `checked` directly as property expressions.
//
// Unlike DockLayoutController's dock panels (which can also be hidden via
// each DockPanel's own close button -- an external mutator the "Side
// Panels" menu items must re-sync against, see MainShell.qml's Connections
// blocks), nothing besides the View->Toolbars menu items themselves ever
// writes to these properties, so no such re-sync is needed here.
//
// All 9 default to hidden, mirroring every toolbar's unconditional
// ->hide() call at the end of MainWindow::setupToolBars().
class NODISCARD_QOBJECT ToolbarLayoutController final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool fileVisible MEMBER m_fileVisible NOTIFY fileVisibleChanged)
    Q_PROPERTY(bool mapperModeVisible MEMBER m_mapperModeVisible NOTIFY mapperModeVisibleChanged)
    Q_PROPERTY(bool mouseModeVisible MEMBER m_mouseModeVisible NOTIFY mouseModeVisibleChanged)
    Q_PROPERTY(bool viewVisible MEMBER m_viewVisible NOTIFY viewVisibleChanged)
    Q_PROPERTY(bool pathMachineVisible MEMBER m_pathMachineVisible NOTIFY pathMachineVisibleChanged)
    Q_PROPERTY(bool roomsVisible MEMBER m_roomsVisible NOTIFY roomsVisibleChanged)
    Q_PROPERTY(bool connectionsVisible MEMBER m_connectionsVisible NOTIFY connectionsVisibleChanged)
    Q_PROPERTY(bool preferencesVisible MEMBER m_preferencesVisible NOTIFY preferencesVisibleChanged)
    Q_PROPERTY(bool audioVisible MEMBER m_audioVisible NOTIFY audioVisibleChanged)

private:
    bool m_fileVisible = false;
    bool m_mapperModeVisible = false;
    bool m_mouseModeVisible = false;
    bool m_viewVisible = false;
    bool m_pathMachineVisible = false;
    bool m_roomsVisible = false;
    bool m_connectionsVisible = false;
    bool m_preferencesVisible = false;
    bool m_audioVisible = false;

public:
    explicit ToolbarLayoutController(QObject *parent = nullptr);

signals:
    void fileVisibleChanged();
    void mapperModeVisibleChanged();
    void mouseModeVisibleChanged();
    void viewVisibleChanged();
    void pathMachineVisibleChanged();
    void roomsVisibleChanged();
    void connectionsVisibleChanged();
    void preferencesVisibleChanged();
    void audioVisibleChanged();
};
