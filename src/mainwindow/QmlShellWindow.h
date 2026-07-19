#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/RuleOf5.h"
#include "../global/Signal2.h"
#include "../global/macros.h"

#include <memory>

#include <QObject>
#include <QPointer>
#include <QString>

class AboutInfo;
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
class FindRoomsController;
class GameObserver;
class GroupController;
class HotkeyManager;
class InfomarkSelection;
class LogModel;
class MapCanvasCore;
class MapData;
class MapSource;
struct MapLoadData;
class MapViewModel;
class MapZoomController;
class MediaLibrary;
class Mmapper2Group;
class MumeClock;
class PreferencesController;
class PrespammedPath;
class QDialog;
class QmlConfig;
class QQmlApplicationEngine;
class QQuickWindow;
class RoomEditController;
class RoomManager;
class RoomModel;
class RoomSelection;
class TasksModel;
class TimerController;
class TimerModel;
class ToolbarLayoutController;
class UiCommand;
class UpdateChecker;
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

public:
    // Widget-free load path: shared by this class's own file.open command
    // (see wireFileCommands()) and main.cpp's tryAutoLoadMap<Shell>()
    // template, which calls this the same way it calls
    // MainWindow::loadFile(). Detects the file format and runs the load on
    // a background thread via async_tasks::startAsyncTask2() (the same
    // engine TasksModel/the Tasks panel already polls -- see
    // QmlShellWindow.h's file comment -- so that panel is this shell's
    // load-progress UI; there is no QProgressDialog here, unlike
    // MainWindow::loadFile()). No-op (after logging) if a load is already
    // running.
    void loadFile(std::shared_ptr<MapSource> source);

private:
    void registerCommands();
    void wireMouseModeCommand(const QString &id, int mode);

    // --- dialogs + window lifecycle (this commit; see the task report) ---
    void wireDialogCommands();
    void wireSelectionCommands();
    void restoreWindowState();
    void persistWindowState();

    // --- file I/O (see the task report's "shell-usable load seam" section)
    // ---
    void wireFileCommands();
    void saveMapFile(const QString &fileName);
    void onSuccessfulLoad(const MapLoadData &mapLoadData);
    // Mirrors MainWindow::updateMapModified()/setMapModified(): re-enables
    // file.save and refreshes the window title based on
    // MapData::dataChanged(), and hides the splash if the map is modified
    // (the "modified" hide trigger; see hideSplash()'s doc comment for the
    // "just loaded" trigger). Connected to MapData::sig_onDataChanged and
    // also called directly after a successful load, mirroring
    // MainWindow::wireConnections()'s sig_onDataChanged lambda and
    // onSuccessfulLoad()'s own updateMapModified() call.
    void updateMapModifiedState();
    void updateWindowTitle();
    // Hides MapView.qml's splash overlay by flipping the "mapLoaded" root
    // context property to true; mirrors MapWindow::hideSplashImage()'s two
    // call sites (mainwindow/utils.cpp's CanvasDisabler destructor -- fires
    // unconditionally after any load/merge attempt, success or failure --
    // and MainWindow::setMapModified(true) -- fires when the map is edited).
    // There is no un-hide: once true, "mapLoaded" (and therefore the splash)
    // never goes back, matching CanvasDisabler/setMapModified's own
    // one-way behavior (a QPointer'd, deleteLater()'d widget on the widget
    // side).
    void hideSplash();

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

    // --- file I/O state (see wireFileCommands()/loadFile()/saveMapFile()) ---
    // True while a load or save task is running in the async_tasks engine;
    // mirrors MainWindow::tryStartNewAsync()'s single-task-at-a-time rule
    // (AsyncIO only ever holds one task), simplified to a flag since this
    // shell has no AsyncIO object of its own.
    bool m_ioInProgress = false;
    bool m_splashHidden = false;

    // --- dialogs (see QmlShellWindow.cpp's wireDialogCommands()/
    // wireSelectionCommands()) ---
    UpdateChecker *m_updateChecker = nullptr;
    QPointer<QDialog> m_updateDialog;

    FindRoomsController *m_findRoomsController = nullptr;
    QPointer<QDialog> m_findRoomsDialog;

    PreferencesController *m_preferencesController = nullptr;
    QPointer<QDialog> m_preferencesDialog;

    // Mirrors MainWindow::m_roomSelection/m_infoMarkSelection: storage for
    // the canvas's current selection, kept here (rather than only inside
    // MapCanvasCore) so room.edit-selected/infomark.edit-selected can be
    // enabled/disabled and the edit dialogs constructed with the right
    // selection, exactly like MainWindow::slot_newRoomSelection()/
    // slot_newInfomarkSelection() do.
    std::shared_ptr<RoomSelection> m_roomSelection;
    std::shared_ptr<InfomarkSelection> m_infoMarkSelection;

    RoomEditController *m_roomEditController = nullptr;
    QPointer<QDialog> m_roomEditDialog;
};
