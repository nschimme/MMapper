#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/RuleOf5.h"
#include "../global/Signal2.h"
#include "../global/macros.h"

#include <memory>

#include <QObject>
#include <QString>

class AdventureLogModel;
class AdventureTracker;
class AudioVolumeController;
class ClientController;
class ClientLineModel;
class ClockAdapter;
class CommandRegistry;
class CTimers;
class DescriptionAdapter;
class DockLayoutController;
class GameObserver;
class GroupController;
class HotkeyManager;
class LogModel;
class MapCanvasCore;
class MapData;
class MapViewModel;
class MapZoomController;
class MediaLibrary;
class Mmapper2Group;
class MumeClock;
class PrespammedPath;
class QmlConfig;
class QQmlApplicationEngine;
class RoomManager;
class RoomModel;
class TasksModel;
class TimerController;
class TimerModel;
class ToolbarLayoutController;
class UiCommand;
class XpStatusAdapter;

// QmlShellWindow bootstraps Shell B, the --qml-shell preview described in
// main.cpp's setSurfaceFormat()/main() (search for MMAPPER_QML_SHELL): a
// minimal, OFFLINE-only bring-up of enough services for MapCanvasCore to
// render and be navigated, a SUBSET of CommandRegistry commands wired to
// real slots, and a QQmlApplicationEngine loading
// qrc:/qt/qml/MMapper/MainShell.qml (source at ../qml/shell/MainShell.qml;
// see src/CMakeLists.txt's QT_RESOURCE_ALIAS comment for why it's aliased
// flat despite living in a "shell/" subdirectory on disk).
//
// Deliberately NOT constructed here (unlike MainWindow): the async task
// engine, the telnet proxy/listener, MPI/remote-edit, the group manager's
// network side, and all file I/O (open/save/merge/export). This is a
// pragmatic first bootable slice, not a feature-complete shell -- see the
// task report for the full list of what's still missing relative to
// MainWindow.
//
// This commit adds the 8 side-panel docks (see MainShell.qml's nested
// SplitView layout and DockPanel.qml/DockColumn.qml/DockRow.qml) and their
// backing models/controllers/adapters, mirroring mainwindow.cpp's dock
// construction block by block. Unlike each widget-shell QmlDockWidget --
// which gets its own private QQuickWidget/QQmlEngine and therefore its own
// isolated root context -- every panel here shares this class's single
// QQmlApplicationEngine, so all of their context properties are set on one
// root context (see the ctor) rather than per-dock. The Client panel's
// ClientController is constructed WITHOUT a ClientControllerBackend (see
// ClientController::setBackend()'s doc comment: every backend-driving
// invokable silently no-ops when none is installed) because this shell has
// no ConnectionListener/telnet proxy to back it with; its "Play" button is
// therefore inert. RoomManager (RoomModel's backing GMCP parser) IS
// constructed here, unlike the note above previously said -- Room panel
// data updates normally, just with nothing upstream ever calling
// GameObserver's GMCP signals in this offline shell.
//
// Owns everything it constructs via plain Qt parent/child (like MainWindow
// does), so its destructor is trivial.
class NODISCARD_QOBJECT QmlShellWindow final : public QObject
{
    Q_OBJECT

public:
    explicit QmlShellWindow(QObject *parent);
    ~QmlShellWindow() final;

    DELETE_CTORS_AND_ASSIGN_OPS(QmlShellWindow);

public:
    // False if MainShell.qml failed to load (e.g. the qrc resource is
    // missing, or a QML syntax error) -- main() should treat that as fatal,
    // matching how setSurfaceFormat() failures are handled.
    NODISCARD bool isValid() const { return m_valid; }

private:
    void registerCommands();
    void wireMouseModeCommand(const QString &id, int mode);

private:
    // --- minimal offline service set (see file comment) ---
    MapData *m_mapData = nullptr;
    PrespammedPath *m_prespammedPath = nullptr;
    Mmapper2Group *m_groupManager = nullptr;
    // GameObserver is not a QObject (see observer/gameobserver.h), so it
    // can't be parented like the other services above; owned via unique_ptr
    // instead, mirroring MainWindow::m_gameObserver.
    std::unique_ptr<GameObserver> m_gameObserver;
    // Lifetime for GameObserver's Signal2 connections (RoomManager's GMCP
    // parsing, AdventureTracker), mirroring MainWindow::m_lifetime.
    Signal2Lifetime m_lifetime;

    MapCanvasCore *m_mapCanvasCore = nullptr;
    MapViewModel *m_mapViewModel = nullptr;
    CommandRegistry *m_commandRegistry = nullptr;

    // --- dock panel backing objects (see file comment) ---
    LogModel *m_logModel = nullptr;

    GroupController *m_groupController = nullptr;
    QmlConfig *m_qmlConfig = nullptr;

    RoomManager *m_roomManager = nullptr;
    RoomModel *m_roomModel = nullptr;

    AdventureTracker *m_adventureTracker = nullptr;
    AdventureLogModel *m_adventureLogModel = nullptr;

    MediaLibrary *m_mediaLibrary = nullptr;
    DescriptionAdapter *m_descriptionAdapter = nullptr;

    CTimers *m_timers = nullptr;
    TimerModel *m_timerModel = nullptr;
    TimerController *m_timerController = nullptr;

    TasksModel *m_tasksModel = nullptr;

    // HotkeyManager is not a QObject (see client/HotkeyManager.h), so it
    // can't be parented like the other services above; owned via
    // unique_ptr instead, mirroring MainWindow::m_hotkeyManager.
    std::unique_ptr<HotkeyManager> m_hotkeyManager;
    ClientLineModel *m_clientLineModel = nullptr;
    ClientController *m_clientController = nullptr;

    DockLayoutController *m_dockLayout = nullptr;
    ToolbarLayoutController *m_toolbarLayout = nullptr;

    // --- toolbar widgets' extracted state (see MapZoomSlider.h/
    // AudioVolumeSlider.h's file comments and the task report for why these
    // are separate classes rather than shared bases) ---
    MapZoomController *m_mapZoomController = nullptr;
    AudioVolumeController *m_musicVolumeController = nullptr;
    AudioVolumeController *m_soundVolumeController = nullptr;

    // --- statusbar widgets (see MainWindow::setupStatusBar()'s
    // MMAPPER_WITH_QML branch, which this mirrors) ---
    MumeClock *m_mumeClock = nullptr;
    ClockAdapter *m_clockAdapter = nullptr;
    XpStatusAdapter *m_xpStatusAdapter = nullptr;

    QQmlApplicationEngine *m_engine = nullptr;
    bool m_valid = false;
};
