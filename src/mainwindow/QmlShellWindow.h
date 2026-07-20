#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/AsyncTasks.h"
#include "../global/ConfigEnums.h"
#include "../global/RuleOf5.h"
#include "../global/Signal2.h"
#include "../global/macros.h"
#include "../mapstorage/MapDestination.h"
#include "../proxy/ProxyHostApi.h"

#include <memory>
#include <optional>

#include <QObject>
#include <QPointer>
#include <QString>

class AboutInfo;
class AdventureLogModel;
class AdventureTracker;
class AppCore;
class AudioManager;
class AudioVolumeController;
class ClientController;
class ClientLineModel;
class ClockAdapter;
class CommandRegistry;
class ConnectionListener;
class ConnectionSelection;
class CTimers;
class DescriptionAdapter;
class DockLayoutController;
class FindRoomsController;
class GameObserver;
class GroupController;
class HotkeyManager;
class InfomarkSelection;
class IoTaskController;
class LogModel;
class Map;
class MapCanvasCore;
class MapData;
class MapSource;
struct MapLoadData;
class MapViewModel;
class MapZoomController;
class MediaLibrary;
class Mmapper2Group;
class Mmapper2PathMachine;
class MumeClock;
class PreferencesController;
class PrespammedPath;
class QDialog;
class QmlConfig;
class QQmlApplicationEngine;
class QQuickWindow;
class QTimer;
class RoomEditController;
class RoomHandle;
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
// bring-up of enough services for MapCanvasCore to render and be navigated,
// a live Mmapper2PathMachine + telnet proxy/listener + client backend, a
// SUBSET of CommandRegistry commands wired to real slots, and a
// QQmlApplicationEngine loading qrc:/qt/qml/MMapper/MainShell.qml (source at
// ../qml/shell/MainShell.qml; see src/CMakeLists.txt's QT_RESOURCE_ALIAS
// comment for why it's aliased flat despite living in a "shell/"
// subdirectory on disk).
//
// Deliberately NOT constructed here (unlike MainWindow): MPI/remote-edit's
// widget-facing pieces beyond what Proxy::allocRemoteEdit() itself needs
// (RemoteEdit's edit-session windows still work -- see ProxyHostApi::
// asQObject() -- but there is no MainWindow-side UI wrapping them here), and
// the group manager's network side. File I/O (open/save/save-as/new/merge/
// reload/the 4 exports, an unsaved-changes maybeSave() guard, and an async
// load/save/merge progress popup) is wired -- see wireFileCommands()/
// confirmClose()/IoTaskController.h. This is a pragmatic bootable slice, not
// a feature-complete shell -- see the task report for the full list of
// what's still missing relative to MainWindow.
//
// This class implements ProxyHostApi (see ../proxy/ProxyHostApi.h) so the
// same ConnectionListener/Proxy graph MainWindow constructs can be
// constructed here too, without Proxy depending on the concrete MainWindow
// type -- see startServices()/slot_log()/slot_setMode()/getHotkeyManager()/
// asQObject() below.
//
// This commit adds the 8 side-panel docks (see MainShell.qml's nested
// SplitView layout and DockPanel.qml/DockColumn.qml/DockRow.qml) and their
// backing models/controllers/adapters, mirroring mainwindow.cpp's dock
// construction block by block. Unlike each widget-shell QmlDockWidget --
// which gets its own private QQuickWidget/QQmlEngine and therefore its own
// isolated root context -- every panel here shares this class's single
// QQmlApplicationEngine, so all of their context properties are set on one
// root context (see the ctor) rather than per-dock. The Client panel's
// ClientController is backed by a real ClientTelnetBackend (see
// startServices()), so its "Play" button drives an actual telnet connection
// through this shell's own ConnectionListener, exactly like MainWindow's.
//
// Owns everything it constructs via plain Qt parent/child (like MainWindow
// does), so its destructor is trivial.
class NODISCARD_QOBJECT QmlShellWindow final : public QObject, public ProxyHostApi
{
    Q_OBJECT

    // --- selection-state properties (this commit) ---
    // Mirror mainwindow.cpp's slot_showContextMenu()'s selection-driven
    // branches (m_roomSelection/m_connectionSelection/m_infoMarkSelection):
    // exposed as properties (rather than plain context values) so
    // MapContextMenu.qml's `visible:` bindings re-evaluate automatically as
    // the canvas selection changes, the same way CommandAction.qml's
    // `enabled:` binding reacts to UiCommand::effectiveEnabled. Updated
    // alongside the existing room/connection/infomark selection tracking in
    // wireSelectionCommands() below.
    Q_PROPERTY(bool hasRoomSelection READ getHasRoomSelection NOTIFY sig_roomSelectionStateChanged)
    Q_PROPERTY(
        bool roomSelectionEmpty READ getRoomSelectionEmpty NOTIFY sig_roomSelectionStateChanged)
    Q_PROPERTY(bool hasConnectionSelection READ getHasConnectionSelection NOTIFY
                   sig_connectionSelectionStateChanged)
    Q_PROPERTY(bool hasInfomarkSelection READ getHasInfomarkSelection NOTIFY
                   sig_infomarkSelectionStateChanged)
    Q_PROPERTY(bool infomarkSelectionEmpty READ getInfomarkSelectionEmpty NOTIFY
                   sig_infomarkSelectionStateChanged)

public:
    explicit QmlShellWindow(QObject *parent);
    ~QmlShellWindow() final;

    DELETE_CTORS_AND_ASSIGN_OPS(QmlShellWindow);

public:
    // --- ProxyHostApi (see ../proxy/ProxyHostApi.h) ---
    void slot_log(const QString &mod, const QString &msg) override;
    void slot_setMode(MapModeEnum mode) override;
    NODISCARD HotkeyManager &getHotkeyManager() const override;
    NODISCARD QObject &asQObject() override { return *this; }

public:
    // False if MainShell.qml failed to load (e.g. the qrc resource is
    // missing, or a QML syntax error) -- main() should treat that as fatal,
    // matching how setSurfaceFormat() failures are handled.
    NODISCARD bool isValid() const { return m_valid; }

public:
    // --- selection-state property readers (see the Q_PROPERTY block above)
    // ---
    NODISCARD bool getHasRoomSelection() const { return m_roomSelection != nullptr; }
    // Defined out-of-line in QmlShellWindow.cpp: RoomSelection/
    // InfomarkSelection are only forward-declared here (see the class
    // forward-decl list above), so empty() can't be called from a header
    // that hasn't included their real definitions.
    NODISCARD bool getRoomSelectionEmpty() const;
    NODISCARD bool getHasConnectionSelection() const { return m_connectionSelection != nullptr; }
    NODISCARD bool getHasInfomarkSelection() const { return m_infoMarkSelection != nullptr; }
    NODISCARD bool getInfomarkSelectionEmpty() const;

public:
    // Widget-free load path: shared by this class's own file.open command
    // (see wireFileCommands()) and main.cpp's tryAutoLoadMap<Shell>()
    // template, which calls this the same way it calls
    // MainWindow::loadFile(). Detects the file format and runs the load on
    // a background thread via async_tasks::startAsyncTask2() (the same
    // engine TasksModel/the Tasks panel already polls -- see
    // QmlShellWindow.h's file comment). Progress is also shown via the modal
    // popup driven by IoTaskController (see beginIoTaskProgress()) -- unlike
    // MainWindow's QProgressDialog, the Tasks panel remains the secondary/
    // historical view, not the only one. No-op (after logging) if a load is
    // already running. Does NOT call maybeSave() itself -- callers that need
    // the unsaved-changes guard (file.open/file.reload) call it first; the
    // auto-load path (main.cpp's tryAutoLoadMap<Shell>()) intentionally
    // skips it, matching MainWindow's own startup auto-load.
    void loadFile(std::shared_ptr<MapSource> source);

public:
    // Invoked from MainShell.qml's Window.onClosing (see the QML file's
    // onClosing handler) for both the [X] titlebar close and file.exit's
    // window->close() (see wireFileCommands()'s file.exit wiring): mirrors
    // MainWindow::closeEvent()'s maybeSave() gate, plus a reduced subset of
    // its "ignore close while a non-cancelable (save) task is running" rule
    // -- unlike MainWindow, this does NOT attempt to cancel a running load/
    // merge and wait for shutdown; it simply refuses to close while any IO
    // task is in flight (see the task report's documented deviations).
    NODISCARD Q_INVOKABLE bool confirmClose();

signals:
    // NOTIFY signals for the selection-state Q_PROPERTYs above; fired from
    // wireSelectionCommands()'s sig_newRoomSelection/sig_newConnectionSelection/
    // sig_newInfomarkSelection handlers, mirroring how MainWindow's
    // slot_showContextMenu() reads m_roomSelection/m_connectionSelection/
    // m_infoMarkSelection live rather than caching a snapshot.
    void sig_roomSelectionStateChanged();
    void sig_connectionSelectionStateChanged();
    void sig_infomarkSelectionStateChanged();

private:
    // Mirrors MainWindow::maybeSave()/mainwindow-saveslots.cpp's
    // MainWindow::maybeSave(): if the map has unsaved changes, asks via a
    // QMessageBox::Save/Discard/Cancel prompt (with MapData::describeChanges()
    // summarizing what changed) whether to save first. Returns false only if
    // the user chose Cancel (or the ensuing save couldn't even start);
    // callers proceed (open/new/reload/merge/quit) whenever this returns
    // true.
    NODISCARD bool maybeSave();
    // Mirrors MainWindow::slot_save()/slot_saveAs(): returns false if a
    // save couldn't even be started (e.g. the save-as dialog was
    // canceled) -- like MainWindow, this does NOT wait for the save to
    // finish before returning true (see saveMapFile()'s async task).
    NODISCARD bool doSave();
    NODISCARD bool doSaveAs();
    // Mirrors MainWindow::forceNewFile(): discards the current map
    // unconditionally (callers are responsible for the maybeSave() guard --
    // see file.new's wiring and loadFile(), which also discards the old map
    // this way before starting a load).
    void forceNewFile();
    // Mirrors MainWindow::slot_merge(): loads fileName in the background and
    // merges it into the current map via maploadhelper::mergeMapData() (see
    // MapLoadHelper.h), showing the same progress popup as loadFile()/
    // saveMapFile() (cancelable, like a load).
    void mergeFile(const QString &fileName);

private:
    // --- progress popup (see IoTaskController.h) ---
    // Starts m_ioProgressTimer and primes m_ioTaskController for a freshly
    // started async_tasks task; called by loadFile()/mergeFile()/
    // saveMapFile() right after async_tasks::startAsyncTask2() returns.
    void beginIoTaskProgress(async_tasks::AsyncTaskHandle handle,
                             const QString &label,
                             bool cancelable);
    // Polls the current task's ProgressCounter, mirroring
    // MainWindow::AsyncIO::updateStatus()'s 25ms QTimer tick; connected to
    // m_ioProgressTimer's timeout().
    void tickIoTaskProgress();
    // Stops m_ioProgressTimer and hides the popup; called from every async
    // task's main-thread completion callback, success or failure.
    void endIoTaskProgress();

private:
    void registerCommands();
    void wireMouseModeCommand(const QString &id, int mode);
    void wireMapperModeCommand(const QString &id, MapModeEnum mode);

    // --- dialogs + window lifecycle (this commit; see the task report) ---
    void wireDialogCommands();
    void wireSelectionCommands();
    void restoreWindowState();
    void persistWindowState();

    // --- path machine + proxy/listener/client (see the task report) ---
    // Mirrors AppCore::setMapperMode(): persists the mode to config, keeps
    // the mapper-mode command group's checked state in sync, and (for
    // MAP/OFFLINE, matching MainWindow::slot_onMapMode()/slot_onOfflineMode())
    // logs the mode switch. Called both from the mapper-mode.* commands'
    // triggered handlers and from slot_setMode() (the mud-driven MPI path).
    void setMapperMode(MapModeEnum mode);
    // Wires Mmapper2PathMachine's player-position signals into the canvas/
    // description panel/status label, mirroring MainWindow::wireConnections()'s
    // pathMachine-facing block.
    void wirePathMachine();
    // Mirrors MainWindow::updateDescriptionRoom(): forwards to
    // m_descriptionAdapter->updateRoom().
    void updateDescriptionRoom(const RoomHandle &room);
    // Constructs the ConnectionListener/Proxy graph and starts listening,
    // mirroring MainWindow::startServices(); called once, at the end of the
    // ctor (this shell has no showEvent() to hook the way MainWindow does --
    // see the ctor's call site for why that's an acceptable substitute).
    void startServices();

    // --- file I/O (see the task report's "shell-usable load seam" section)
    // ---
    void wireFileCommands();
    // mode/format generalize this beyond plain MM2 saves so the 4
    // file.export.* commands (see wireFileCommands()) can share this same
    // async-task/progress-popup machinery, mirroring MainWindow::saveFile()'s
    // per-format storage selection (mainwindow-async.cpp).
    void saveMapFile(const QString &fileName, SaveModeEnum mode, SaveFormatEnum format);
    void onSuccessfulLoad(const MapLoadData &mapLoadData);
    // Mirrors MainWindow::onSuccessfulMerge(), minus mapChanged() (widget-
    // only canvas repaint; MapCanvasCore::slot_dataLoaded() already covers
    // the mesh rebuild that matters here -- see onSuccessfulLoad()'s doc
    // comment for the same tradeoff).
    void onSuccessfulMerge(const Map &map);
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
    // Mirrors MainWindow's m_appCore: the shell-agnostic slice of selection
    // -> command-enabling logic (AppCore::onNewRoomSelection()/
    // onNewConnectionSelection()/onNewInfomarkSelection()/
    // updateRoomOffsetCommands(), see AppCore.h) is reused verbatim here --
    // it only touches MapData/Mmapper2PathMachine/CommandRegistry, never the
    // widget-only MapCanvas (see AppCore::setCanvas()'s doc comment), so it
    // is already shell-agnostic despite living in the same directory as the
    // widget-only MainWindow. Its setCanvasMouseMode()/layerUp()/layerDown()/
    // layerReset() methods ARE widget-only (they call through m_canvas,
    // which is never set here -- see setCanvas()'s doc comment); this shell
    // continues to wire mouse-mode.*/layer.* commands directly to
    // MapCanvasCore instead (see registerCommands()/wireMouseModeCommand()).
    AppCore *m_appCore = nullptr;

    Mmapper2PathMachine *m_pathMachine = nullptr;
    AudioManager *m_audioManager = nullptr;
    // Owned by this class (like MainWindow::m_listener); constructed by
    // startServices() once the rest of the service graph exists (needs
    // m_mumeClock/m_timers, both constructed later in the ctor).
    ConnectionListener *m_listener = nullptr;

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
    // True while a QMessageBox::exec() from confirmClose()/maybeSave() is on
    // the stack, re-entrancy guard for onClosing() firing again if a nested
    // event loop lets the window receive another close request while the
    // prompt is still open (mirrors the fact that MainWindow's closeEvent()
    // is never reentered while its own nested QMessageBox::exec() runs, but
    // spelled out explicitly here since QML's onClosing has no equivalent
    // built-in guard).
    bool m_confirmingClose = false;

    // --- progress popup state (see IoTaskController.h/beginIoTaskProgress()/
    // tickIoTaskProgress()/endIoTaskProgress()) ---
    IoTaskController *m_ioTaskController = nullptr;
    QTimer *m_ioProgressTimer = nullptr;
    // The task currently driving the popup; unset between tasks. Only ever
    // holds a Load/Merge/Save task started by this class's own file I/O
    // paths (see QmlShellWindow.h's file comment: this shell has no other
    // async_tasks::IO producer).
    std::optional<async_tasks::AsyncTaskHandle> m_ioTaskHandle;

    // --- dialogs (see QmlShellWindow.cpp's wireDialogCommands()/
    // wireSelectionCommands()) ---
    UpdateChecker *m_updateChecker = nullptr;
    QPointer<QDialog> m_updateDialog;

    FindRoomsController *m_findRoomsController = nullptr;
    QPointer<QDialog> m_findRoomsDialog;

    PreferencesController *m_preferencesController = nullptr;
    QPointer<QDialog> m_preferencesDialog;

    // Mirrors MainWindow::m_roomSelection/m_connectionSelection/
    // m_infoMarkSelection: storage for the canvas's current selection, kept
    // here (rather than only inside MapCanvasCore) so room.edit-selected/
    // infomark.edit-selected can be enabled/disabled and the edit dialogs
    // constructed with the right selection, exactly like
    // MainWindow::slot_newRoomSelection()/slot_newConnectionSelection()/
    // slot_newInfomarkSelection() do. m_connectionSelection additionally
    // backs connection.delete-selected and MapContextMenu.qml's
    // hasConnectionSelection property (see the Q_PROPERTY block above).
    std::shared_ptr<RoomSelection> m_roomSelection;
    std::shared_ptr<ConnectionSelection> m_connectionSelection;
    std::shared_ptr<InfomarkSelection> m_infoMarkSelection;

    RoomEditController *m_roomEditController = nullptr;
    QPointer<QDialog> m_roomEditDialog;
};
