// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "QmlShellWindow.h"

#include "../adventure/AdventureLogModel.h"
#include "../adventure/XpStatusAdapter.h"
#include "../adventure/adventuretracker.h"
#include "../client/ClientController.h"
#include "../client/ClientLineModel.h"
#include "../client/ClientTelnetBackend.h"
#include "../client/HotkeyManager.h"
#include "../clock/ClockAdapter.h"
#include "../clock/mumeclock.h"
#include "../configuration/configuration.h"
#include "../display/CanvasMouseModeEnum.h"
#include "../display/InfomarkSelection.h"
#include "../display/MapCanvasCore.h"
#include "../display/MapViewModel.h"
#include "../display/MapZoomController.h"
#include "../display/connectionselection.h"
#include "../display/prespammedpath.h"
#include "../global/AsyncTasks.h"
#include "../global/ConfigConsts.h"
#include "../global/TextUtils.h"
#include "../global/Version.h"
#include "../global/utils.h"
#include "../group/GroupController.h"
#include "../group/GroupModel.h"
#include "../group/mmapper2group.h"
#include "../map/RoomHandle.h"
#include "../map/mmapper2room.h"
#include "../mapdata/mapdata.h"
#include "../mapdata/roomselection.h"
#include "../mapstorage/MapDestination.h"
#include "../mapstorage/MapLoadHelper.h"
#include "../mapstorage/MapSource.h"
#include "../mapstorage/MmpMapStorage.h"
#include "../mapstorage/XmlMapStorage.h"
#include "../mapstorage/abstractmapstorage.h"
#include "../mapstorage/jsonmapstorage.h"
#include "../mapstorage/mapstorage.h"
#include "../media/AudioManager.h"
#include "../media/DescriptionAdapter.h"
#include "../media/MediaLibrary.h"
#include "../observer/gameobserver.h"
#include "../pathmachine/mmapper2pathmachine.h"
#include "../preferences/GeneralPageAdapter.h"
#include "../preferences/ParserPageAdapter.h"
#include "../preferences/PreferencesController.h"
#include "../proxy/GmcpMessage.h"
#include "../proxy/connectionlistener.h"
#include "../qml/AnsiColorPickerLauncher.h"
#include "../qml/DescriptionImageProvider.h"
#include "../qml/DockLayoutController.h"
#include "../qml/GroupIconProvider.h"
#include "../qml/PasswordDialogLauncher.h"
#include "../qml/QmlConfig.h"
#include "../qml/QmlDialog.h"
#include "../qml/ToolbarLayoutController.h"
#include "../roompanel/RoomManager.h"
#include "../roompanel/RoomModel.h"
#include "../timers/CTimers.h"
#include "../timers/TimerController.h"
#include "../timers/TimerModel.h"
#include "../viewers/TopLevelWindows.h"
#include "AboutInfo.h"
#include "AppCore.h"
#include "AudioVolumeController.h"
#include "CommandRegistry.h"
#include "FindRoomsController.h"
#include "FindRoomsModel.h"
#include "InfomarkEditController.h"
#include "IoTaskController.h"
#include "LogModel.h"
#include "RoomEditController.h"
#include "TasksModel.h"
#include "UiCommand.h"
#include "UpdateChecker.h"
#include "metatypes.h"

#include <algorithm>
#include <array>
#include <future>
#include <memory>
#include <optional>
#include <tuple>
#include <utility>

#include <QApplication>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QCursor>
#include <QDataStream>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QMessageBox>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QRect>
#include <QScreen>
#include <QTimer>
#include <QToolTip>
#include <QUrl>

namespace { // anonymous

struct NODISCARD CommandSpec final
{
    const char *id = nullptr;
    bool checkable = false;
    // Mirrors the QAction text/shortcut MainWindow::createActions() sets on
    // the equivalent action (see mainwindow.cpp); shown by CommandAction.qml
    // even for commands registered disabled below, so every menu item has a
    // real label instead of the blank text an unset UiCommand::text would
    // otherwise show. Ampersand mnemonics are kept verbatim (QQC2.MenuItem
    // honors them the same way QAction does); empty means "no shortcut", not
    // "unset".
    const char *text = "";
    const char *shortcut = "";
};

// The full command surface MainWindow::createActions() registers (see
// CommandRegistry.h's id-scheme doc comment for the authoritative
// namespace list). QmlShellWindow registers every one of these so
// CommandAction.qml's `commands.command(id)` never resolves to null for a
// known id -- but only the ids in LIVE_COMMAND_IDS/MOUSE_MODE_COMMANDS
// below (below) are actually wired to a working slot and left enabled;
// everything else is registered disabled as a placeholder for a later
// commit (file I/O, room/connection/infomark editing, the client, group
// manager UI, help links, ...) that needs MainWindow's still-widget-only
// machinery (QFileDialog, QMessageBox, the async task engine, the proxy/
// listener, ...). edit.undo/edit.redo deliberately leave shortcut empty:
// mainwindow.cpp binds those to the platform-varying QKeySequence::Undo/
// Redo standard keys, not a literal string, and there's no live undo/redo
// stack behind this offline shell yet for a shortcut to usefully trigger.
constexpr std::array<CommandSpec, 65> ALL_COMMAND_SPECS = {
    CommandSpec{"file.new", false, "&New", ""},
    CommandSpec{"file.open", false, "&Open...", "Ctrl+O"},
    CommandSpec{"file.merge", false, "&Merge...", ""},
    CommandSpec{"file.reload", false, "&Reload", "Ctrl+R"},
    CommandSpec{"file.save", false, "&Save", "Ctrl+S"},
    CommandSpec{"file.save-as", false, "Save &As...", ""},
    CommandSpec{"file.export.base-map", false, "Export MMapper2 &Base Map As...", ""},
    CommandSpec{"file.export.mm2xml", false, "Export MMapper2 &XML Map As...", ""},
    CommandSpec{"file.export.web", false, "Export &Web Map As...", ""},
    CommandSpec{"file.export.mmp", false, "Export &MMP Map As...", ""},
    CommandSpec{"file.exit", false, "E&xit", "Ctrl+Q"},
    CommandSpec{"edit.undo", false, "&Undo", ""},
    CommandSpec{"edit.redo", false, "&Redo", ""},
    CommandSpec{"edit.preferences", false, "&Preferences", "Ctrl+P"},
    CommandSpec{"view.zoom-in", false, "Zoom In", "Ctrl++"},
    CommandSpec{"view.zoom-out", false, "Zoom Out", "Ctrl+-"},
    CommandSpec{"view.zoom-reset", false, "Zoom Reset", "Ctrl+0"},
    CommandSpec{"view.always-on-top", true, "Always On Top", ""},
    CommandSpec{"view.show-status-bar", true, "Always Show Status Bar", ""},
    CommandSpec{"view.show-scroll-bars", true, "Always Show Scrollbars", ""},
    CommandSpec{"view.show-menu-bar", true, "Always Show Menubar", ""},
    CommandSpec{"layer.up", false, "Layer Up", "Ctrl+Tab"},
    CommandSpec{"layer.down", false, "Layer Down", "Ctrl+Shift+Tab"},
    CommandSpec{"layer.reset", false, "Layer Reset", ""},
    CommandSpec{"mouse-mode.move", true, "Move map", ""},
    CommandSpec{"mouse-mode.room-raypick", true, "Ray-pick Rooms", ""},
    CommandSpec{"mouse-mode.room-select", true, "Select Rooms", ""},
    CommandSpec{"mouse-mode.connection-select", true, "Select Connection", ""},
    CommandSpec{"mouse-mode.create-room", true, "Create New Rooms", ""},
    CommandSpec{"mouse-mode.create-connection", true, "Create New Connection", ""},
    CommandSpec{"mouse-mode.create-oneway-connection", true, "Create New Oneway Connection", ""},
    CommandSpec{"mouse-mode.infomark-select", true, "Select Markers", ""},
    CommandSpec{"mouse-mode.create-infomark", true, "Create New Markers", ""},
    CommandSpec{"mapper-mode.play", true, "Switch to play mode", ""},
    CommandSpec{"mapper-mode.map", true, "Switch to mapping mode", ""},
    CommandSpec{"mapper-mode.offline", true, "Switch to offline emulation mode", ""},
    CommandSpec{"room.create", false, "Create New Room", ""},
    CommandSpec{"room.edit-selected", false, "Edit Selected Rooms", "Ctrl+E"},
    CommandSpec{"room.delete-selected", false, "Delete Selected Rooms", ""},
    CommandSpec{"room.move-up-selected", false, "Move Up Selected Rooms", ""},
    CommandSpec{"room.move-down-selected", false, "Move Down Selected Rooms", ""},
    CommandSpec{"room.merge-up-selected", false, "Merge Up Selected Rooms", ""},
    CommandSpec{"room.merge-down-selected", false, "Merge Down Selected Rooms", ""},
    CommandSpec{"room.connect-to-neighbours", false, "Connect room(s) to its neighbour rooms", ""},
    CommandSpec{"room.find", false, "&Find Rooms", "Ctrl+F"},
    CommandSpec{"room.goto-selected", false, "Move to selected room", ""},
    CommandSpec{"room.force-update-selected", false, "Update selected room with last movement", ""},
    CommandSpec{"connection.delete-selected", false, "Delete Selected Connection", ""},
    CommandSpec{"infomark.edit-selected", true, "Edit Selected Markers", ""},
    CommandSpec{"infomark.delete-selected", true, "Delete Selected Markers", ""},
    CommandSpec{"client.launch", false, "&Launch mud client", ""},
    CommandSpec{"client.save-log", false, "Save Log as &Plain Text...", ""},
    CommandSpec{"client.save-log-html", false, "Save Log as &HTML...", ""},
    CommandSpec{"pathmachine.release-all-paths", false, "Release All Paths", ""},
    CommandSpec{"world.rebuild-meshes", false, "&Rebuild World", ""},
    CommandSpec{"help.vote", false, "V&ote for Mume", ""},
    CommandSpec{"help.check-for-update", false, "Check for &update", ""},
    CommandSpec{"help.website", false, "&Website", ""},
    CommandSpec{"help.forum", false, "&Forum", ""},
    CommandSpec{"help.wiki", false, "W&iki", ""},
    CommandSpec{"help.setup", false, "Get &Help", ""},
    CommandSpec{"help.newbie", false, "&Information for Newcomers", ""},
    CommandSpec{"help.report-issue", false, "Report an &Issue...", ""},
    CommandSpec{"help.about", false, "About &MMapper", ""},
    CommandSpec{"help.about-qt", false, "About &Qt", ""},
};

// Commands wired to a real slot below (registerCommands()/
// wireMouseModeCommand()); everything in ALL_COMMAND_SPECS not listed here is
// registered but left disabled. Kept as a helper function (rather than a
// second constexpr array) so it can use QLatin1String comparisons directly
// against CommandSpec::id without another array-of-pointers to keep in
// sync by hand.
NODISCARD bool isLiveCommand(const QString &id)
{
    static const QStringList live{
        "file.new",
        "file.open",
        "file.merge",
        "file.reload",
        "file.save",
        "file.save-as",
        "file.export.base-map",
        "file.export.mm2xml",
        "file.export.web",
        "file.export.mmp",
        "file.exit",
        "edit.undo",
        "edit.redo",
        "view.zoom-in",
        "view.zoom-out",
        "view.zoom-reset",
        "view.always-on-top",
        "view.show-status-bar",
        "view.show-scroll-bars",
        "view.show-menu-bar",
        "layer.up",
        "layer.down",
        "layer.reset",
        "world.rebuild-meshes",
        "mapper-mode.play",
        "mapper-mode.map",
        "mapper-mode.offline",
        "pathmachine.release-all-paths",
        "client.launch",
        "client.save-log",
        "client.save-log-html",
        "room.goto-selected",
        "room.force-update-selected",
        "mouse-mode.move",
        "mouse-mode.room-raypick",
        "mouse-mode.room-select",
        "mouse-mode.connection-select",
        "mouse-mode.create-room",
        "mouse-mode.create-connection",
        "mouse-mode.create-oneway-connection",
        "mouse-mode.infomark-select",
        "mouse-mode.create-infomark",
        "room.find",
        "edit.preferences",
        "help.about",
        "help.about-qt",
        "help.check-for-update",
        "help.vote",
        "help.website",
        "help.forum",
        "help.wiki",
        "help.setup",
        "help.newbie",
        "help.report-issue",
        // room.edit-selected/infomark.edit-selected are registered live but
        // START disabled (see registerCommands()'s
        // "no selection yet" comment below) -- wireSelectionCommands()
        // toggles them as the canvas selection changes, mirroring
        // MainWindow::slot_newRoomSelection()/slot_newInfomarkSelection()'s
        // m_commandRegistry->command(...)->setEnabled() calls (see
        // AppCore::onNewRoomSelection()).
        "room.edit-selected",
        "infomark.edit-selected",
        // room.create/delete-selected/move-*-selected/merge-*-selected/
        // connect-to-neighbours, connection.delete-selected, and
        // infomark.delete-selected are registered live but (except
        // room.create) START disabled -- wireSelectionCommands() enables
        // them as the canvas selection changes, mirroring
        // MainWindow::createActions()'s selectedRoomActGroup/
        // selectedConnectionActGroup/infomarkGroup->setEnabled(false)
        // (see this file's wireSelectionCommands()).
        "room.create",
        "room.delete-selected",
        "room.move-up-selected",
        "room.move-down-selected",
        "room.merge-up-selected",
        "room.merge-down-selected",
        "room.connect-to-neighbours",
        "connection.delete-selected",
        "infomark.delete-selected",
    };
    return live.contains(id);
}

// Parity with ConfigDialog::closeEvent()/mainwindow.cpp's
// PreferencesQmlDialog (see slot_onPreferences() there): closing the
// preferences window via the title bar persists the (apply-on-change)
// settings to disk; only the explicit Cancel button reverts.
class NODISCARD_QOBJECT PreferencesQmlDialog final : public QmlDialog
{
public:
    using QmlDialog::QmlDialog;

protected:
    void closeEvent(QCloseEvent *const event) override
    {
        getConfig().write();
        QmlDialog::closeEvent(event);
    }
};

// Mirrors mainwindow.cpp's RoomEditQmlDialog (see slot_onEditRoomSelection()
// there): refuses the [X]/Alt+F4 close path while the note has unsaved
// edits, but lets Escape's reject() through (after warning about the
// about-to-be-discarded note text).
class NODISCARD_QOBJECT RoomEditQmlDialog final : public QmlDialog
{
private:
    RoomEditController *m_controller = nullptr;

public:
    explicit RoomEditQmlDialog(RoomEditController *const controller,
                               const QString &title,
                               const QString &objectName,
                               QWidget *const parent)
        : QmlDialog(title, objectName, parent)
        , m_controller(controller)
    {}

protected:
    void closeEvent(QCloseEvent *const event) override
    {
        if (m_controller != nullptr && m_controller->getNoteDirty()) {
            deref(event).ignore();
            return;
        }
        QmlDialog::closeEvent(event);
    }

    void reject() override
    {
        if (m_controller != nullptr && m_controller->getNoteDirty()) {
            auto *const box = new QMessageBox(this);
            box->setAttribute(Qt::WA_DeleteOnClose);
            box->setWindowTitle(QObject::tr("[mmapper] warning: ignored note"));
            box->setText(m_controller->getNoteText());
            box->open();
        }
        QmlDialog::reject();
    }
};

// QRect<->QByteArray codec for DockLayoutController's per-dock
// xFloatGeometry properties (../qml/DockLayoutController.h) and
// Configuration::QmlShellSettings's matching dockXFloatGeometry fields
// (../configuration/configuration.h) -- same QDataStream round-trip
// restoreWindowState()/persistWindowState() already use for the top-level
// window's own geometry, reused here so a floating dock's Window restores
// its last position/size the same way the shell's own top-level window
// does. An empty/invalid input decodes to a default-constructed (invalid)
// QRect, which MainShell.qml's FloatingDock treats as "no saved geometry".
NODISCARD QByteArray encodeFloatGeometry(const QRect &rect)
{
    QByteArray bytes;
    QDataStream out(&bytes, QIODevice::WriteOnly);
    out << rect;
    return bytes;
}

NODISCARD QRect decodeFloatGeometry(const QByteArray &bytes)
{
    if (bytes.isEmpty()) {
        return {};
    }
    QDataStream in(bytes);
    QRect rect;
    in >> rect;
    if (in.status() != QDataStream::Ok) {
        return {};
    }
    return rect;
}

} // namespace

QmlShellWindow::QmlShellWindow(QObject *const parent)
    : QObject(parent)
{
    // --- process-global initialization MainWindow's constructor performs
    // (mainwindow.cpp): the async task engine MUST be initialized before
    // anything schedules work on it -- MapCanvasCore's map remesh runs
    // through async_tasks, and without init() the worker-side machinery
    // derefs an unset global, which threw an uncaught NullPointerException
    // on a background thread and aborted the process the first time the
    // shell painted a map. registerMetatypes() is likewise required for
    // queued cross-thread signal arguments, initTopLevelWindows() backs
    // the ANSI viewer registry, and the bundled monospace font matches
    // MainWindow's addApplicationFont(). ---
    initTopLevelWindows();
    async_tasks::init();
    registerMetatypes();
    std::ignore = QFontDatabase::addApplicationFont(":/fonts/DejaVuSansMono.ttf");

    // --- minimal offline service set MapCanvasCore's constructor needs
    // (see display/MapCanvasCore.h) -- mirrors the construction order in
    // MainWindow::MainWindow() (mainwindow.cpp), minus everything that
    // needs the proxy/listener or QtWidgets. ---
    m_commandRegistry = new CommandRegistry(this);

    m_mapData = new MapData(this);
    m_mapData->setObjectName("MapData");

    m_prespammedPath = new PrespammedPath(this);
    m_groupManager = new Mmapper2Group(this);
    m_groupManager->setObjectName("GroupManager");
    m_gameObserver = std::make_unique<GameObserver>();

    m_mapCanvasCore = new MapCanvasCore(deref(m_mapData),
                                        deref(m_gameObserver),
                                        deref(m_prespammedPath),
                                        deref(m_groupManager),
                                        this);

    m_mapViewModel = new MapViewModel(this);
    connect(m_mapCanvasCore,
            &MapCanvasCore::sig_setScrollBars,
            m_mapViewModel,
            &MapViewModel::slot_setScrollBars);
    // Remaining canvas <-> scroll-model wiring, mirroring MapWindow's ctor
    // "from canvas to map window" / "from map window to canvas" blocks
    // (mapwindow.cpp:150-186) -- MapWindow's sig_setScrollBars connection is
    // already mirrored just above; these cover the rest: drag-pan feedback
    // (sig_mapMove), edge auto-scroll (sig_continuousScroll), the room-find/
    // path-machine "center on this position" path (sig_onCenter), and the
    // resulting scroll position being pushed back into the canvas
    // (sig_scrollToWorld/sig_scrollWorldXChanged/sig_scrollWorldYChanged --
    // see MapViewModel.h's doc comments on those signals for exactly which
    // MapWindow lambda/slot each one replaces).
    connect(m_mapCanvasCore,
            &MapCanvasCore::sig_onCenter,
            m_mapViewModel,
            &MapViewModel::slot_centerOnWorldPos);
    connect(m_mapCanvasCore,
            &MapCanvasCore::sig_mapMove,
            m_mapViewModel,
            &MapViewModel::slot_mapMove);
    connect(m_mapCanvasCore,
            &MapCanvasCore::sig_continuousScroll,
            m_mapViewModel,
            &MapViewModel::slot_continuousScroll);
    connect(m_mapViewModel,
            &MapViewModel::sig_scrollToWorld,
            m_mapCanvasCore,
            &MapCanvasCore::slot_setScroll);
    connect(m_mapViewModel,
            &MapViewModel::sig_scrollWorldXChanged,
            m_mapCanvasCore,
            &MapCanvasCore::slot_setHorizontalScroll);
    connect(m_mapViewModel,
            &MapViewModel::sig_scrollWorldYChanged,
            m_mapCanvasCore,
            &MapCanvasCore::slot_setVerticalScroll);

    m_pathMachine = new Mmapper2PathMachine(deref(m_mapData), this);
    m_pathMachine->setObjectName("Mmapper2PathMachine");
    connect(m_mapData,
            &MapFrontend::sig_clearingMap,
            m_pathMachine,
            &PathMachine::slot_releaseAllPaths);

    // See QmlShellWindow.h's m_appCore doc comment for why this reuses
    // AppCore::onNewRoomSelection()/onNewConnectionSelection()/
    // onNewInfomarkSelection()/updateRoomOffsetCommands() verbatim rather
    // than re-implementing the selection -> command-enabling logic here.
    // setCanvas() is deliberately never called -- its mouse-mode/layer
    // methods are unused on this path (see the header comment).
    m_appCore = new AppCore(deref(m_mapData), deref(m_pathMachine), deref(m_commandRegistry), this);
    connect(m_appCore, &AppCore::sig_statusMessage, this, [this](const QString &text, int) {
        if (m_engine != nullptr) {
            deref(m_engine->rootContext()).setContextProperty("statusText", text);
        }
    });

    registerCommands();

    // --- dock panel backing objects (see file comment) -- mirrors
    // mainwindow.cpp's per-panel std::invoke() blocks, minus the
    // #ifdef MMAPPER_WITH_QML widget-shell branches (this class only ever
    // builds under WITH_QML; see QmlShellWindow.h's ctor comment). ---
    m_qmlConfig = new QmlConfig(this);

    m_logModel = new LogModel(this);

    m_groupController = new GroupController(deref(m_groupManager), deref(m_mapData), this);

    m_roomManager = new RoomManager(this);
    m_roomManager->setObjectName("RoomManager");
    m_gameObserver->sig2_sentToUserGmcp.connect(m_lifetime, [this](const GmcpMessage &gmcp) {
        deref(m_roomManager).slot_parseGmcpInput(gmcp);
    });
    m_roomModel = new RoomModel(this, deref(m_roomManager).getRoom());
    connect(m_roomManager, &RoomManager::sig_updateWidget, m_roomModel, &RoomModel::update);

    m_adventureTracker = new AdventureTracker(deref(m_gameObserver), this);
    m_adventureLogModel = new AdventureLogModel(deref(m_adventureTracker), this);

    m_mediaLibrary = new MediaLibrary(this);
    m_descriptionAdapter = new DescriptionAdapter(deref(m_mediaLibrary), this);
    m_audioManager = new AudioManager(deref(m_mediaLibrary), deref(m_gameObserver), this);

    m_timers = new CTimers(this);
    m_timerModel = new TimerModel(deref(m_timers), this);
    m_timerController = new TimerController(deref(m_timers), deref(m_timerModel), this);

    m_tasksModel = new TasksModel(this);

    m_hotkeyManager = std::make_unique<HotkeyManager>();
    m_clientLineModel = new ClientLineModel(this);
    m_clientController = new ClientController(deref(m_clientLineModel),
                                              deref(m_hotkeyManager),
                                              this);
    // ClientControllerBackend is installed by startServices() below, once
    // m_listener exists (ClientTelnetBackend needs a ConnectionListener to
    // back it with -- see ClientController::setBackend()'s doc comment and
    // startServices()).

    m_dockLayout = new DockLayoutController(this);
    m_toolbarLayout = new ToolbarLayoutController(this);

    // --- toolbar widgets' extracted state (see MapZoomController.h/
    // AudioVolumeController.h's file comments and the task report) ---
    m_mapZoomController = new MapZoomController(deref(m_mapCanvasCore), this);
    m_musicVolumeController = new AudioVolumeController(AudioVolumeController::AudioType::Music,
                                                        this);
    m_soundVolumeController = new AudioVolumeController(AudioVolumeController::AudioType::Sound,
                                                        this);

    // --- statusbar widgets -- mirrors MainWindow::setupStatusBar()'s
    // MMAPPER_WITH_QML branch (mainwindow.cpp) constructing ClockAdapter/
    // XpStatusAdapter, minus the QQuickWidget hosting (ClockStrip.qml/
    // XpStatusItem.qml are instantiated directly in MainShell.qml's footer
    // instead) and minus ClockAdapter::setToolTipWidget(): there is no
    // QWidget to give it here (this is a QQuickWindow, not a QQuickWidget),
    // but showToolTip() already falls back to QCursor::pos() with no
    // hosting widget (see ClockAdapter.cpp and
    // TestQml::clockAdapterNativeToolTip(), which exists specifically to
    // cover that fallback), so the native QToolTip this shows still works
    // correctly -- just anchored at the cursor instead of at the clock
    // chip's own screen position. Least-churn choice over adding a
    // QML-side ToolTip fallback: ClockStrip.qml is shared between both
    // shells and already routes every hover through this adapter.
    m_mumeClock = new MumeClock(getConfig().mumeClock.startEpoch, deref(m_gameObserver), this);
    m_clockAdapter = new ClockAdapter(deref(m_gameObserver), deref(m_mumeClock), this);
    m_xpStatusAdapter = new XpStatusAdapter(deref(m_adventureTracker), this);
    connect(m_xpStatusAdapter, &XpStatusAdapter::sig_toggleAdventurePanel, this, [this]() {
        const bool wasVisible = m_dockLayout->property("adventureVisible").toBool();
        m_dockLayout->setProperty("adventureVisible", !wasVisible);
    });

    m_engine = new QQmlApplicationEngine(this);
    m_engine->rootContext()->setContextProperty("commands", m_commandRegistry);
    m_engine->rootContext()->setContextProperty("mapCore", m_mapCanvasCore);
    m_engine->rootContext()->setContextProperty("mapViewModel", m_mapViewModel);
    // Idle status text, mirroring MainWindow::setupStatusBar()'s
    // `m_appCore->showStatusForever(tr("Say friend and enter..."))`
    // (mainwindow.cpp:2002) exactly. AppCore::sig_statusMessage (connected
    // above, right after m_appCore's construction) is the sole driver of
    // "statusText" from here on -- this just seeds it with the same idle
    // string shell A shows before anything else has posted a status
    // message; the lambda needs a non-null m_engine to write the context
    // property, so this can't run any earlier than here.
    m_appCore->showStatusForever(tr("Say friend and enter..."));
    // Splash-overlay visibility (MapView.qml) and window title (MainShell.qml's
    // ApplicationWindow.title) -- see hideSplash()/updateWindowTitle(), called
    // from the file I/O paths wired below in wireFileCommands().
    m_engine->rootContext()->setContextProperty("mapLoaded", false);
    // Selects MapView.qml's canvas host type: MapCanvasUnderlay
    // (MapCanvasUnderlayItem.h -- draws the map directly into the window's
    // framebuffer beneath the rest of the Quick scene, the performance
    // end-state) by default, or MapCanvasItem (MapCanvasQuickItem.h, the
    // older QQuickFramebufferObject item-owns-its-own-FBO path) when the
    // MMAPPER_CANVAS_FBO=1 environment variable is set -- an escape hatch
    // for a platform/driver combination where the beforeRenderPassRecording()
    // underlay misbehaves. See MapView.qml's file comment for how this
    // property is consumed, and MapCanvasUnderlayItem.h's file comment for
    // why the underlay is the preferred default.
    m_engine->rootContext()->setContextProperty("useCanvasUnderlay",
                                                qgetenv("MMAPPER_CANVAS_FBO") != "1");
    m_engine->rootContext()->setContextProperty("windowTitle",
                                                QStringLiteral("MMapper (QML shell preview)"));
    // Path-machine status label (MainShell.qml's footer pathMachineLabel) --
    // replaces the former static placeholder; kept in sync with
    // Mmapper2PathMachine::sig_state by wirePathMachine() below.
    m_engine->rootContext()->setContextProperty("pathMachineStatus", QString());

    // --- dock panel context properties -- one root context shared by every
    // panel (see file comment for why this differs from QmlDockWidget's
    // per-dock setContextProperty()). Image providers must be registered
    // before the engine loads MainShell.qml. ---
    m_engine->addImageProvider(QStringLiteral("groupicons"), new GroupIconProvider());
    m_engine->addImageProvider(QStringLiteral("description"),
                               new DescriptionImageProvider(m_descriptionAdapter->getStore()));

    QQmlContext *const rootContext = m_engine->rootContext();
    rootContext->setContextProperty("dockLayout", m_dockLayout);
    rootContext->setContextProperty("config", m_qmlConfig);

    rootContext->setContextProperty("logModel", m_logModel);

    rootContext->setContextProperty("groupModel", m_groupController->getModel());
    rootContext->setContextProperty("groupProxyModel", m_groupController->getProxy());
    rootContext->setContextProperty("groupController", m_groupController);

    rootContext->setContextProperty("roomModel", m_roomModel);

    rootContext->setContextProperty("adventureLogModel", m_adventureLogModel);

    rootContext->setContextProperty("adapter", m_descriptionAdapter);

    rootContext->setContextProperty("timerModel", m_timerModel);
    rootContext->setContextProperty("timerController", m_timerController);

    rootContext->setContextProperty("tasksModel", m_tasksModel);

    rootContext->setContextProperty("clientController", m_clientController);
    rootContext->setContextProperty("clientLineModel", m_clientLineModel);

    // --- async IO progress popup (see IoTaskController.h and
    // beginIoTaskProgress()/tickIoTaskProgress()/endIoTaskProgress() below)
    // ---
    m_ioTaskController = new IoTaskController(this);
    rootContext->setContextProperty("ioTask", m_ioTaskController);
    connect(m_ioTaskController, &IoTaskController::sig_cancelRequested, this, [this]() {
        if (m_ioTaskHandle) {
            m_ioTaskHandle->requestCancel();
        }
    });
    m_ioProgressTimer = new QTimer(this);
    m_ioProgressTimer->setInterval(25);
    connect(m_ioProgressTimer, &QTimer::timeout, this, &QmlShellWindow::tickIoTaskProgress);

    // "shell" -- this object itself, exposed for MainShell.qml's
    // Window.onClosing to call confirmClose() (see this class's header doc
    // comment); a minimal facade, mirroring how "commands"/"dockLayout"
    // above expose one purpose-built object rather than the whole class.
    rootContext->setContextProperty("shell", this);

    // --- toolbar/statusbar context properties (see this ctor's toolbar-
    // widget/statusbar-widget construction blocks above) ---
    rootContext->setContextProperty("toolbarLayout", m_toolbarLayout);
    rootContext->setContextProperty("mapZoom", m_mapZoomController);
    rootContext->setContextProperty("musicVolume", m_musicVolumeController);
    rootContext->setContextProperty("soundVolume", m_soundVolumeController);
    rootContext->setContextProperty("clock", m_clockAdapter);
    // XpStatusItem.qml's expected context property is "xpStatusAdapter", not
    // "adapter" -- MainWindow::setupStatusBar() originally named it "adapter"
    // on xpQuick's own private per-widget engine (no collision there), but
    // "adapter" is already bound to m_descriptionAdapter on this shell's one
    // shared root context (see above, shared by DescriptionPanel.qml), so
    // XpStatusItem.qml/mainwindow.cpp/TestQml.cpp's loadXpStatusItem() were
    // all renamed together to "xpStatusAdapter" to resolve the collision;
    // see the task report.
    rootContext->setContextProperty("xpStatusAdapter", m_xpStatusAdapter);

    // Funnels a few real signals into the footer's status message, mirroring
    // (in miniature) MainWindow::setupStatusBar()'s AppCore::showStatusForever()
    // / XpStatusAdapter show/clear-status-message wiring. setContextProperty()
    // re-fires bindings that read "statusText" the same way a Q_PROPERTY
    // NOTIFY would. sig_clearStatusMessage routes back through
    // AppCore::showStatusForever() (rather than setting the context property
    // directly) so AppCore stays the single source of truth for the idle
    // string (mainwindow.cpp:2002) -- see this ctor's m_appCore->
    // showStatusForever() call above.
    connect(m_xpStatusAdapter,
            &XpStatusAdapter::sig_showStatusMessage,
            this,
            [rootContext](const QString &msg) {
                rootContext->setContextProperty("statusText", msg);
            });
    connect(m_xpStatusAdapter, &XpStatusAdapter::sig_clearStatusMessage, this, [this]() {
        m_appCore->showStatusForever(tr("Say friend and enter..."));
    });
    connect(m_commandRegistry->command("world.rebuild-meshes"),
            &UiCommand::sig_triggered,
            this,
            [rootContext]() {
                rootContext->setContextProperty("statusText",
                                                QStringLiteral("Rebuilding the world mesh..."));
            });

    // --- dialogs + selection-driven commands (see this class's
    // wireDialogCommands()/wireSelectionCommands()) -- registered before
    // load() since they only touch m_commandRegistry/m_mapCanvasCore, not
    // the QML root object. ---
    wireDialogCommands();
    wireSelectionCommands();
    wireFileCommands();
    wirePathMachine();

    // Restore Shell B's dock/toolbar visibility from Configuration::qmlShell
    // *before* load(): DockLayoutController/ToolbarLayoutController's
    // properties are read by MainShell.qml's SplitView/toolbar bindings at
    // creation time, so setting them beforehand avoids a visible flash from
    // the compiled-in default to the persisted value.
    const auto &qmlShellConfig = getConfig().qmlShell;
    m_dockLayout->setProperty("logVisible", qmlShellConfig.dockLogVisible);
    m_dockLayout->setProperty("groupVisible", qmlShellConfig.dockGroupVisible);
    m_dockLayout->setProperty("roomVisible", qmlShellConfig.dockRoomVisible);
    m_dockLayout->setProperty("adventureVisible", qmlShellConfig.dockAdventureVisible);
    m_dockLayout->setProperty("descriptionVisible", qmlShellConfig.dockDescriptionVisible);
    m_dockLayout->setProperty("timersVisible", qmlShellConfig.dockTimersVisible);
    m_dockLayout->setProperty("tasksVisible", qmlShellConfig.dockTasksVisible);
    m_dockLayout->setProperty("clientVisible", qmlShellConfig.dockClientVisible);
    m_dockLayout->setProperty("logFloating", qmlShellConfig.dockLogFloating);
    m_dockLayout->setProperty("groupFloating", qmlShellConfig.dockGroupFloating);
    m_dockLayout->setProperty("roomFloating", qmlShellConfig.dockRoomFloating);
    m_dockLayout->setProperty("adventureFloating", qmlShellConfig.dockAdventureFloating);
    m_dockLayout->setProperty("descriptionFloating", qmlShellConfig.dockDescriptionFloating);
    m_dockLayout->setProperty("timersFloating", qmlShellConfig.dockTimersFloating);
    m_dockLayout->setProperty("tasksFloating", qmlShellConfig.dockTasksFloating);
    m_dockLayout->setProperty("clientFloating", qmlShellConfig.dockClientFloating);
    m_dockLayout->setProperty("logFloatGeometry",
                              decodeFloatGeometry(qmlShellConfig.dockLogFloatGeometry));
    m_dockLayout->setProperty("groupFloatGeometry",
                              decodeFloatGeometry(qmlShellConfig.dockGroupFloatGeometry));
    m_dockLayout->setProperty("roomFloatGeometry",
                              decodeFloatGeometry(qmlShellConfig.dockRoomFloatGeometry));
    m_dockLayout->setProperty("adventureFloatGeometry",
                              decodeFloatGeometry(qmlShellConfig.dockAdventureFloatGeometry));
    m_dockLayout->setProperty("descriptionFloatGeometry",
                              decodeFloatGeometry(qmlShellConfig.dockDescriptionFloatGeometry));
    m_dockLayout->setProperty("timersFloatGeometry",
                              decodeFloatGeometry(qmlShellConfig.dockTimersFloatGeometry));
    m_dockLayout->setProperty("tasksFloatGeometry",
                              decodeFloatGeometry(qmlShellConfig.dockTasksFloatGeometry));
    m_dockLayout->setProperty("clientFloatGeometry",
                              decodeFloatGeometry(qmlShellConfig.dockClientFloatGeometry));
    // Restore each dock's area (see DockLayoutController::setDockArea());
    // order relative to the visibility/floating restores above doesn't
    // matter -- setDockArea() calls recomputeAreaLists() itself, which
    // reads whatever xVisible/xFloating state is current at the time.
    m_dockLayout->setDockArea(QStringLiteral("log"), qmlShellConfig.dockLogArea);
    m_dockLayout->setDockArea(QStringLiteral("group"), qmlShellConfig.dockGroupArea);
    m_dockLayout->setDockArea(QStringLiteral("room"), qmlShellConfig.dockRoomArea);
    m_dockLayout->setDockArea(QStringLiteral("adventure"), qmlShellConfig.dockAdventureArea);
    m_dockLayout->setDockArea(QStringLiteral("description"), qmlShellConfig.dockDescriptionArea);
    m_dockLayout->setDockArea(QStringLiteral("timers"), qmlShellConfig.dockTimersArea);
    m_dockLayout->setDockArea(QStringLiteral("tasks"), qmlShellConfig.dockTasksArea);
    m_dockLayout->setDockArea(QStringLiteral("client"), qmlShellConfig.dockClientArea);
    m_toolbarLayout->setProperty("fileVisible", qmlShellConfig.toolbarFileVisible);
    m_toolbarLayout->setProperty("mapperModeVisible", qmlShellConfig.toolbarMapperModeVisible);
    m_toolbarLayout->setProperty("mouseModeVisible", qmlShellConfig.toolbarMouseModeVisible);
    m_toolbarLayout->setProperty("viewVisible", qmlShellConfig.toolbarViewVisible);
    m_toolbarLayout->setProperty("pathMachineVisible", qmlShellConfig.toolbarPathMachineVisible);
    m_toolbarLayout->setProperty("roomsVisible", qmlShellConfig.toolbarRoomsVisible);
    m_toolbarLayout->setProperty("connectionsVisible", qmlShellConfig.toolbarConnectionsVisible);
    m_toolbarLayout->setProperty("preferencesVisible", qmlShellConfig.toolbarPreferencesVisible);
    m_toolbarLayout->setProperty("audioVisible", qmlShellConfig.toolbarAudioVisible);

    // Restore view.always-on-top/show-status-bar/show-scroll-bars from the
    // widget shell's own GeneralSettings keys (deliberately shared with
    // MainWindow -- see configuration.h's Configuration::QmlShellSettings
    // doc comment: only the window geometry/dock/toolbar layout needs a
    // separate key namespace to avoid clobbering the two shells' state;
    // these three plain toggles are fine to share, same as MainWindow's own
    // slot_alwaysOnTop()/readSettings() do).
    if (UiCommand *const cmd = m_commandRegistry->command("view.always-on-top")) {
        cmd->setChecked(getConfig().general.alwaysOnTop);
    }
    if (UiCommand *const cmd = m_commandRegistry->command("view.show-status-bar")) {
        cmd->setChecked(getConfig().general.showStatusBar);
    }
    if (UiCommand *const cmd = m_commandRegistry->command("view.show-scroll-bars")) {
        cmd->setChecked(getConfig().general.showScrollBars);
    }
    if (UiCommand *const cmd = m_commandRegistry->command("view.show-menu-bar")) {
        cmd->setChecked(getConfig().general.showMenuBar);
    }
    connect(m_commandRegistry->command("view.always-on-top"),
            &UiCommand::sig_triggered,
            this,
            [this]() {
                UiCommand *const cmd = m_commandRegistry->command("view.always-on-top");
                const bool alwaysOnTop = cmd != nullptr && cmd->isChecked();
                setConfig().general.alwaysOnTop = alwaysOnTop;
                if (auto *const window = qobject_cast<QQuickWindow *>(
                        m_engine->rootObjects().isEmpty() ? nullptr
                                                          : m_engine->rootObjects().constFirst())) {
                    window->setFlag(Qt::WindowStaysOnTopHint, alwaysOnTop);
                }
            });
    connect(m_commandRegistry->command("view.show-status-bar"),
            &UiCommand::sig_triggered,
            this,
            [this]() {
                UiCommand *const cmd = m_commandRegistry->command("view.show-status-bar");
                setConfig().general.showStatusBar = cmd != nullptr && cmd->isChecked();
            });
    connect(m_commandRegistry->command("view.show-scroll-bars"),
            &UiCommand::sig_triggered,
            this,
            [this]() {
                UiCommand *const cmd = m_commandRegistry->command("view.show-scroll-bars");
                setConfig().general.showScrollBars = cmd != nullptr && cmd->isChecked();
            });
    // view.show-menu-bar -- mirrors MainWindow::slot_setShowMenuBar()
    // (mainwindow.cpp:1889-1919), minus the auto-reveal-on-hover event-filter
    // dance (m_dockDialog*->installEventFilter()/setMouseTracking()): this
    // shell has no widget event-filter equivalent yet. TODO(future shell
    // commit): reproduce auto-reveal-on-hover for the QML menu bar.
    connect(m_commandRegistry->command("view.show-menu-bar"),
            &UiCommand::sig_triggered,
            this,
            [this]() {
                UiCommand *const cmd = m_commandRegistry->command("view.show-menu-bar");
                setConfig().general.showMenuBar = cmd != nullptr && cmd->isChecked();
            });

    m_engine->load(QUrl(QStringLiteral("qrc:/qt/qml/MMapper/MainShell.qml")));
    m_valid = !m_engine->rootObjects().isEmpty();

    restoreWindowState();

    // Persist geometry + dock/toolbar visibility on quit. Shell B still
    // retains QApplication (see QmlDialog's file comment / the task
    // report's QmlDialog-under-GL-shell reasoning), so
    // QCoreApplication::aboutToQuit() fires reliably both for file.exit's
    // qApp->quit() and for the user closing the last top-level window
    // (QApplication's default quitOnLastWindowClosed) -- this mirrors
    // MainWindow::closeEvent()'s writeSettings() call without needing a
    // widget closeEvent() override here (this class owns a QQuickWindow via
    // the engine, not a QWidget). aboutToQuit() fires while the window is
    // still alive, so persistWindowState() can still read its geometry.
    connect(qApp, &QCoreApplication::aboutToQuit, this, &QmlShellWindow::persistWindowState);

    // Mirrors MainWindow::showEvent()'s std::call_once(startServices())
    // (mainwindow.cpp): this shell has no showEvent() to hook (it owns a
    // QQuickWindow via the engine, not a QWidget), and the window is shown
    // by QQmlApplicationEngine's Window::visible: true as soon as load()
    // above returns, so calling this once here at the end of the ctor is an
    // acceptable substitute -- there is no earlier point at which the
    // service graph (m_mumeClock/m_timers/m_pathMachine/m_mapCanvasCore)
    // isn't fully constructed yet.
    startServices();

    // Restore the mapper mode from config, mirroring MainWindow's ctor
    // switch on getConfig().general.mapMode (mainwindow.cpp): must run after
    // startServices() so a PLAY-mode mud connection request from a mud that
    // races the proxy coming up still finds the group/telnet stack ready.
    setMapperMode(getConfig().general.mapMode);
}

QmlShellWindow::~QmlShellWindow()
{
    // The engine must go FIRST, explicitly: QObject::deleteChildren()
    // destroys children in construction order, and MapCanvasCore is
    // constructed long before the engine -- so left to the base-class
    // destructor, the core would be freed while the engine's
    // ApplicationWindow (and its scene graph, including the
    // MapCanvasQuickItem's node whose teardown calls back into the core)
    // is still alive. That exact ordering was a heap-use-after-free under
    // ASan (MapCanvasCore::isCleanedUp() read during
    // QQuickWindowPrivate::cleanupNodesOnShutdown()). Destroying the engine
    // here tears the whole QML scene down while the core and every
    // context-property object are still valid.
    delete m_engine;
    m_engine = nullptr;

    // Mirrors MainWindow's destructor (mainwindow.cpp): stop and drain the
    // async task engine before tearing down the objects its workers may
    // still reference, then release the top-level window registry.
    async_tasks::cleanup();
    destroyTopLevelWindows();
}

void QmlShellWindow::restoreWindowState()
{
    if (m_engine == nullptr || m_engine->rootObjects().isEmpty()) {
        return;
    }
    auto *const window = qobject_cast<QQuickWindow *>(m_engine->rootObjects().constFirst());
    if (window == nullptr) {
        return;
    }
    // QQuickWindow (unlike QWidget) has no restoreGeometry()/saveGeometry()
    // pair, so this class round-trips a plain QRect through QDataStream
    // instead -- simpler than the widget shell's opaque, platform-specific
    // blob, and sufficient for a single top-level window with no dock/
    // toolbar area state baked into it (that's DockLayoutController/
    // ToolbarLayoutController's job, persisted separately above).
    const QByteArray &geometry = getConfig().qmlShell.geometry;
    bool restored = false;
    if (!geometry.isEmpty()) {
        QDataStream in(geometry);
        QRect rect;
        in >> rect;
        if (in.status() == QDataStream::Ok && rect.isValid()) {
            window->setGeometry(rect);
            restored = true;
        }
    }
    if (!restored) {
        // First run (no stored qmlShell geometry yet): mirrors
        // MainWindow::readSettings()'s firstRun branch (mainwindow.cpp),
        // which centers the window on the primary screen via
        // QStyle::alignedRect(..., size(), primaryScreen()->availableGeometry()).
        // QQuickWindow has no QStyle::alignedRect() equivalent, so this
        // centers by hand using the window's own size (already set from
        // MainShell.qml's Window{width;height} defaults by the time
        // load() returns, above) against the primary screen's available
        // geometry.
        if (QScreen *const screen = QGuiApplication::primaryScreen()) {
            const QRect avail = screen->availableGeometry();
            const QSize size = window->size();
            window->setPosition(avail.x() + (avail.width() - size.width()) / 2,
                                avail.y() + (avail.height() - size.height()) / 2);
        }
    }
    window->setFlag(Qt::WindowStaysOnTopHint, getConfig().general.alwaysOnTop);
}

void QmlShellWindow::persistWindowState()
{
    if (m_engine == nullptr || m_engine->rootObjects().isEmpty()) {
        return;
    }
    auto *const window = qobject_cast<QQuickWindow *>(m_engine->rootObjects().constFirst());
    if (window == nullptr) {
        return;
    }

    auto &qmlShellConfig = setConfig().qmlShell;
    {
        QByteArray geometry;
        QDataStream out(&geometry, QIODevice::WriteOnly);
        out << window->geometry();
        qmlShellConfig.geometry = geometry;
    }
    qmlShellConfig.dockLogVisible = m_dockLayout->property("logVisible").toBool();
    qmlShellConfig.dockGroupVisible = m_dockLayout->property("groupVisible").toBool();
    qmlShellConfig.dockRoomVisible = m_dockLayout->property("roomVisible").toBool();
    qmlShellConfig.dockAdventureVisible = m_dockLayout->property("adventureVisible").toBool();
    qmlShellConfig.dockDescriptionVisible = m_dockLayout->property("descriptionVisible").toBool();
    qmlShellConfig.dockTimersVisible = m_dockLayout->property("timersVisible").toBool();
    qmlShellConfig.dockTasksVisible = m_dockLayout->property("tasksVisible").toBool();
    qmlShellConfig.dockClientVisible = m_dockLayout->property("clientVisible").toBool();
    qmlShellConfig.dockLogFloating = m_dockLayout->property("logFloating").toBool();
    qmlShellConfig.dockGroupFloating = m_dockLayout->property("groupFloating").toBool();
    qmlShellConfig.dockRoomFloating = m_dockLayout->property("roomFloating").toBool();
    qmlShellConfig.dockAdventureFloating = m_dockLayout->property("adventureFloating").toBool();
    qmlShellConfig.dockDescriptionFloating = m_dockLayout->property("descriptionFloating").toBool();
    qmlShellConfig.dockTimersFloating = m_dockLayout->property("timersFloating").toBool();
    qmlShellConfig.dockTasksFloating = m_dockLayout->property("tasksFloating").toBool();
    qmlShellConfig.dockClientFloating = m_dockLayout->property("clientFloating").toBool();
    qmlShellConfig.dockLogFloatGeometry = encodeFloatGeometry(
        m_dockLayout->property("logFloatGeometry").toRect());
    qmlShellConfig.dockGroupFloatGeometry = encodeFloatGeometry(
        m_dockLayout->property("groupFloatGeometry").toRect());
    qmlShellConfig.dockRoomFloatGeometry = encodeFloatGeometry(
        m_dockLayout->property("roomFloatGeometry").toRect());
    qmlShellConfig.dockAdventureFloatGeometry = encodeFloatGeometry(
        m_dockLayout->property("adventureFloatGeometry").toRect());
    qmlShellConfig.dockDescriptionFloatGeometry = encodeFloatGeometry(
        m_dockLayout->property("descriptionFloatGeometry").toRect());
    qmlShellConfig.dockTimersFloatGeometry = encodeFloatGeometry(
        m_dockLayout->property("timersFloatGeometry").toRect());
    qmlShellConfig.dockTasksFloatGeometry = encodeFloatGeometry(
        m_dockLayout->property("tasksFloatGeometry").toRect());
    qmlShellConfig.dockClientFloatGeometry = encodeFloatGeometry(
        m_dockLayout->property("clientFloatGeometry").toRect());
    qmlShellConfig.dockLogArea = m_dockLayout->dockArea(QStringLiteral("log"));
    qmlShellConfig.dockGroupArea = m_dockLayout->dockArea(QStringLiteral("group"));
    qmlShellConfig.dockRoomArea = m_dockLayout->dockArea(QStringLiteral("room"));
    qmlShellConfig.dockAdventureArea = m_dockLayout->dockArea(QStringLiteral("adventure"));
    qmlShellConfig.dockDescriptionArea = m_dockLayout->dockArea(QStringLiteral("description"));
    qmlShellConfig.dockTimersArea = m_dockLayout->dockArea(QStringLiteral("timers"));
    qmlShellConfig.dockTasksArea = m_dockLayout->dockArea(QStringLiteral("tasks"));
    qmlShellConfig.dockClientArea = m_dockLayout->dockArea(QStringLiteral("client"));
    qmlShellConfig.toolbarFileVisible = m_toolbarLayout->property("fileVisible").toBool();
    qmlShellConfig.toolbarMapperModeVisible = m_toolbarLayout->property("mapperModeVisible").toBool();
    qmlShellConfig.toolbarMouseModeVisible = m_toolbarLayout->property("mouseModeVisible").toBool();
    qmlShellConfig.toolbarViewVisible = m_toolbarLayout->property("viewVisible").toBool();
    qmlShellConfig.toolbarPathMachineVisible = m_toolbarLayout->property("pathMachineVisible")
                                                   .toBool();
    qmlShellConfig.toolbarRoomsVisible = m_toolbarLayout->property("roomsVisible").toBool();
    qmlShellConfig.toolbarConnectionsVisible = m_toolbarLayout->property("connectionsVisible")
                                                   .toBool();
    qmlShellConfig.toolbarPreferencesVisible = m_toolbarLayout->property("preferencesVisible")
                                                   .toBool();
    qmlShellConfig.toolbarAudioVisible = m_toolbarLayout->property("audioVisible").toBool();

    setConfig().write();
}

void QmlShellWindow::wireMouseModeCommand(const QString &id, const int mode)
{
    UiCommand *const cmd = m_commandRegistry->command(id);
    if (cmd == nullptr) {
        return;
    }
    m_commandRegistry->addToGroup(cmd, "mouse-mode", true);
    const auto canvasMouseMode = static_cast<CanvasMouseModeEnum>(mode);
    connect(cmd, &UiCommand::sig_triggered, this, [this, cmd, canvasMouseMode]() {
        m_mapCanvasCore->slot_setCanvasMouseMode(canvasMouseMode);
        cmd->setChecked(true);
    });
}

void QmlShellWindow::wireMapperModeCommand(const QString &id, const MapModeEnum mode)
{
    UiCommand *const cmd = m_commandRegistry->command(id);
    if (cmd == nullptr) {
        return;
    }
    m_commandRegistry->addToGroup(cmd, "mapper-mode", true);
    connect(cmd, &UiCommand::sig_triggered, this, [this, mode]() { setMapperMode(mode); });
}

void QmlShellWindow::setMapperMode(const MapModeEnum mode)
{
    // Mirrors AppCore::setMapperMode() (persist to config) plus
    // MainWindow::slot_onMapMode()/slot_onOfflineMode()'s logging (there is
    // no MainWindow-only modeMenu icon to update here -- see
    // QmlShellWindow.h's file comment -- MainShell.qml's CommandAction
    // bindings already reflect the checked command directly).
    setConfig().general.mapMode = mode;
    getConfig().write();

    const char *cmdId = nullptr;
    switch (mode) {
    case MapModeEnum::PLAY:
        cmdId = "mapper-mode.play";
        break;
    case MapModeEnum::MAP:
        cmdId = "mapper-mode.map";
        slot_log("QmlShellWindow",
                 "Map mode selected - new rooms are created when entering unmapped areas.");
        break;
    case MapModeEnum::OFFLINE:
        cmdId = "mapper-mode.offline";
        slot_log("QmlShellWindow", "Offline emulation mode selected - learn new areas safely.");
        break;
    }
    if (cmdId != nullptr) {
        if (UiCommand *const cmd = m_commandRegistry->command(QString::fromUtf8(cmdId))) {
            cmd->setChecked(true);
        }
    }
}

void QmlShellWindow::slot_log(const QString &mod, const QString &msg)
{
    // Mirrors MainWindow::slot_log()'s MMAPPER_WITH_QML branch: forwards to
    // the Log panel's model. MainWindow additionally forwards to
    // statusBar() via a short-lived message; this shell has no equivalent
    // "log line as transient status" behavior yet (statusText is reserved
    // for the funneled signals in the ctor), so that part is skipped.
    if (m_logModel != nullptr) {
        m_logModel->append(mod, msg);
    }
}

void QmlShellWindow::slot_setMode(const MapModeEnum mode)
{
    // Mirrors MainWindow::slot_setMode(): the mud can request a mapper-mode
    // switch via MPI (see ProxyMudConnectionApi::virt_onSetMode() in
    // proxy.cpp).
    setMapperMode(mode);
}

HotkeyManager &QmlShellWindow::getHotkeyManager() const
{
    return deref(m_hotkeyManager);
}

void QmlShellWindow::updateDescriptionRoom(const RoomHandle &room)
{
    deref(m_descriptionAdapter).updateRoom(room);
}

void QmlShellWindow::wirePathMachine()
{
    // Mirrors MainWindow::wireConnections()'s Mmapper2PathMachine-facing
    // block (mainwindow.cpp): moves the canvas's marker, refreshes the
    // description panel + audio area, and keeps the path-machine status
    // label (MainShell.qml's footer pathMachineLabel, via the
    // "pathMachineStatus" context property) in sync.
    connect(m_pathMachine,
            &Mmapper2PathMachine::sig_playerMoved,
            m_mapCanvasCore,
            &MapCanvasCore::slot_moveMarker);
    connect(m_pathMachine, &Mmapper2PathMachine::sig_playerMoved, this, [this](const RoomId &id) {
        if (const auto room = m_mapData->getRoomHandle(id)) {
            updateDescriptionRoom(room);
            m_audioManager->onAreaChanged(room.getArea());
        }
    });
    connect(m_mapData, &MapData::sig_onPositionChange, this, [this]() {
        m_pathMachine->onPositionChange(m_mapData->getCurrentRoomId());
        updateDescriptionRoom(m_mapData->getCurrentRoom());
        m_audioManager->onAreaChanged(m_mapData->getCurrentRoom().getArea());
    });
    connect(m_mapData,
            &MapData::sig_onForcedPositionChange,
            m_mapCanvasCore,
            &MapCanvasCore::slot_onForcedPositionChange);

    connect(m_pathMachine, &Mmapper2PathMachine::sig_state, this, [this](const QString &text) {
        deref(m_engine->rootContext()).setContextProperty("pathMachineStatus", text);
    });
}

void QmlShellWindow::startServices()
{
    // Mirrors MainWindow::startServices() (mainwindow.cpp), minus the
    // update-checker branch (already handled by wireDialogCommands()'s
    // help.check-for-update wiring; MainWindow only special-cases the
    // startup auto-check, which this shell doesn't perform -- documented
    // deviation, see the task report).
    auto *const listener = new ConnectionListener(deref(m_mapData),
                                                  deref(m_pathMachine),
                                                  deref(m_prespammedPath),
                                                  deref(m_groupManager),
                                                  deref(m_mumeClock),
                                                  deref(m_timers),
                                                  deref(m_mapCanvasCore),
                                                  deref(m_gameObserver),
                                                  this);
    connect(listener, &ConnectionListener::sig_log, this, &QmlShellWindow::slot_log);
    connect(listener, &ConnectionListener::sig_clientSuccessfullyConnected, this, [this]() {
        if (!m_clientController->getUsingClient()) {
            m_dockLayout->setProperty("clientVisible", false);
        }
    });
    // Mirrors ClientWidget::slot_onVisibilityChanged()/MainWindow's
    // m_dockDialogClient->visibilityChanged wiring (mainwindow.cpp): delay
    // 500ms to distinguish a real hide/show of the Client dock from it
    // briefly popping back in, then disconnect if hidden-while-connected or
    // focus the input if shown-while-disconnected.
    connect(m_dockLayout, &DockLayoutController::clientVisibleChanged, this, [this]() {
        if (!m_clientController->getUsingClient()) {
            return;
        }
        QTimer::singleShot(500, this, [this]() {
            const bool visible = m_dockLayout->property("clientVisible").toBool();
            if (m_clientController->getConnected() && !visible) {
                m_clientController->disconnectFromMud();
            } else if (!m_clientController->getConnected() && visible) {
                emit m_clientController->sig_requestInputFocus();
            }
        });
    });
    connect(m_clientController,
            &ClientController::sig_relayMessage,
            this,
            [this](const QString &message) {
                deref(m_engine->rootContext()).setContextProperty("statusText", message);
            });
    m_listener = listener;

    m_clientController->setBackend(
        std::make_unique<ClientTelnetBackend>(deref(m_listener), deref(m_clientController)));

    try {
        m_listener->listen();
        slot_log("ConnectionListener",
                 tr("Server bound on localhost to port: %1.").arg(getConfig().connection.localPort));
    } catch (const std::exception &e) {
        const QString errorMsg = tr("Unable to start the server (switching to offline mode): %1.")
                                     .arg(QString::fromUtf8(e.what()));
        QMessageBox::critical(nullptr, tr("mmapper"), errorMsg);
        deref(m_engine->rootContext()).setContextProperty("statusText", errorMsg);
    }
}

void QmlShellWindow::registerCommands()
{
    for (const CommandSpec &spec : ALL_COMMAND_SPECS) {
        const QString id = QString::fromUtf8(spec.id);
        UiCommand *const cmd = m_commandRegistry->addCommand(id, spec.checkable);
        cmd->setText(QString::fromUtf8(spec.text));
        if (*spec.shortcut != '\0') {
            cmd->setShortcut(QString::fromUtf8(spec.shortcut));
        }
        if (!isLiveCommand(id)) {
            // TODO(future shell commit): wire this command to a real
            // implementation (most need MainWindow's still-widget-only
            // machinery: QFileDialog/QMessageBox, the async task engine, or
            // the proxy/listener -- see QmlShellWindow.h's file comment).
            cmd->setEnabled(false);
        }
    }

    // Mirrors mainwindow.cpp's exitAct -> close() wiring (QMainWindow::close()
    // triggers closeEvent(), which is where MainWindow::maybeSave() lives):
    // routes through the top-level window's close() rather than calling
    // qApp->quit() directly, so MainShell.qml's Window.onClosing (and
    // therefore confirmClose()/maybeSave()) gets a chance to veto -- a bare
    // quit() stops the event loop without dispatching a close event at all.
    connect(m_commandRegistry->command("file.exit"), &UiCommand::sig_triggered, this, [this]() {
        if (m_engine != nullptr && !m_engine->rootObjects().isEmpty()) {
            if (auto *const window = qobject_cast<QQuickWindow *>(
                    m_engine->rootObjects().constFirst())) {
                window->close();
                return;
            }
        }
        qApp->quit();
    });

    // edit.undo/edit.redo -- mirrors mainwindow.cpp's m_undoAction/
    // m_redoAction wiring (mainwindow.cpp:1003-1022): both start disabled
    // and stay in sync with MapData::sig_undoAvailable/sig_redoAvailable
    // (MapData inherits MapFrontend, which owns the undo/redo history and
    // these signals -- see mapfrontend.h).
    if (UiCommand *const cmd = m_commandRegistry->command("edit.undo")) {
        cmd->setEnabled(false);
        connect(cmd, &UiCommand::sig_triggered, m_mapData, &MapData::slot_undo);
        connect(m_mapData, &MapData::sig_undoAvailable, cmd, &UiCommand::setEnabled);
    }
    if (UiCommand *const cmd = m_commandRegistry->command("edit.redo")) {
        cmd->setEnabled(false);
        connect(cmd, &UiCommand::sig_triggered, m_mapData, &MapData::slot_redo);
        connect(m_mapData, &MapData::sig_redoAvailable, cmd, &UiCommand::setEnabled);
    }

    connect(m_commandRegistry->command("view.zoom-in"),
            &UiCommand::sig_triggered,
            m_mapCanvasCore,
            &MapCanvasCore::slot_zoomIn);
    connect(m_commandRegistry->command("view.zoom-out"),
            &UiCommand::sig_triggered,
            m_mapCanvasCore,
            &MapCanvasCore::slot_zoomOut);
    connect(m_commandRegistry->command("view.zoom-reset"),
            &UiCommand::sig_triggered,
            m_mapCanvasCore,
            &MapCanvasCore::slot_zoomReset);

    connect(m_commandRegistry->command("layer.up"),
            &UiCommand::sig_triggered,
            m_mapCanvasCore,
            &MapCanvasCore::slot_layerUp);
    connect(m_commandRegistry->command("layer.down"),
            &UiCommand::sig_triggered,
            m_mapCanvasCore,
            &MapCanvasCore::slot_layerDown);
    connect(m_commandRegistry->command("layer.reset"),
            &UiCommand::sig_triggered,
            m_mapCanvasCore,
            &MapCanvasCore::slot_layerReset);

    connect(m_commandRegistry->command("world.rebuild-meshes"),
            &UiCommand::sig_triggered,
            m_mapCanvasCore,
            &MapCanvasCore::slot_rebuildMeshes);

    // Mouse-mode group: mirrors MainWindow::createActions()'s "mouse-mode"
    // QActionGroup (see mainwindow.cpp), enum order taken from
    // CanvasMouseModeEnum.h.
    wireMouseModeCommand("mouse-mode.move", static_cast<int>(CanvasMouseModeEnum::MOVE));
    wireMouseModeCommand("mouse-mode.room-raypick",
                         static_cast<int>(CanvasMouseModeEnum::RAYPICK_ROOMS));
    wireMouseModeCommand("mouse-mode.room-select",
                         static_cast<int>(CanvasMouseModeEnum::SELECT_ROOMS));
    wireMouseModeCommand("mouse-mode.connection-select",
                         static_cast<int>(CanvasMouseModeEnum::SELECT_CONNECTIONS));
    wireMouseModeCommand("mouse-mode.create-room",
                         static_cast<int>(CanvasMouseModeEnum::CREATE_ROOMS));
    wireMouseModeCommand("mouse-mode.create-connection",
                         static_cast<int>(CanvasMouseModeEnum::CREATE_CONNECTIONS));
    wireMouseModeCommand("mouse-mode.create-oneway-connection",
                         static_cast<int>(CanvasMouseModeEnum::CREATE_ONEWAY_CONNECTIONS));
    wireMouseModeCommand("mouse-mode.infomark-select",
                         static_cast<int>(CanvasMouseModeEnum::SELECT_INFOMARKS));
    wireMouseModeCommand("mouse-mode.create-infomark",
                         static_cast<int>(CanvasMouseModeEnum::CREATE_INFOMARKS));

    // Default mouse mode, mirroring MainWindow::createActions()'s
    // mouseMode.modeMoveSelectAct->setChecked(true).
    if (UiCommand *const moveCmd = m_commandRegistry->command("mouse-mode.move")) {
        moveCmd->setChecked(true);
    }

    // Mapper-mode group: mirrors MainWindow::createActions()'s "mapper-mode"
    // QActionGroup (see mainwindow.cpp's mapperMode.playModeAct/mapModeAct/
    // offlineModeAct). The initial checked state is set from config at the
    // end of the ctor (see setMapperMode(getConfig().general.mapMode) there)
    // rather than here, since PLAY mode needs startServices()'s proxy/
    // listener to already exist.
    wireMapperModeCommand("mapper-mode.play", MapModeEnum::PLAY);
    wireMapperModeCommand("mapper-mode.map", MapModeEnum::MAP);
    wireMapperModeCommand("mapper-mode.offline", MapModeEnum::OFFLINE);

    // pathmachine.release-all-paths -- mirrors MainWindow::createActions()'s
    // releaseAllPathsAct.
    connect(m_commandRegistry->command("pathmachine.release-all-paths"),
            &UiCommand::sig_triggered,
            m_pathMachine,
            &PathMachine::slot_releaseAllPaths);

    // client.launch -- mirrors MainWindow::slot_onLaunchClient(), which just
    // shows/raises the m_dockDialogClient QDockWidget (mainwindow.cpp:1388-
    // 1390). This shell has no QDockWidget for the client -- it's a
    // DockLayoutController-tracked panel (ClientPanel.qml) instead -- so
    // "show/raise" becomes "make the dock visible and un-float it", the same
    // property-setting pattern MainShell.qml's own client dock-panel menu
    // item toggle already uses (see the "Panel" MenuItems' `dockLayout.
    // clientVisible = checked` bindings).
    connect(m_commandRegistry->command("client.launch"), &UiCommand::sig_triggered, this, [this]() {
        m_dockLayout->setProperty("clientVisible", true);
        m_dockLayout->setProperty("clientFloating", false);
    });

    // client.save-log/client.save-log-html -- mirror mainwindow.cpp's own
    // MMAPPER_WITH_QML branch for saveLogAct/saveLogAsHtmlAct
    // (mainwindow.cpp:1388-1425), which already reads from
    // m_clientLineModel (ClientLineModel::toPlainText()/toHtml(), see
    // ClientLineModel.h) instead of ClientWidget::slot_saveLog()/
    // slot_saveLogAsHtml()'s QTextDocument -- reused verbatim here since
    // QmlShellWindow owns the same m_clientLineModel.
    connect(m_commandRegistry->command("client.save-log"), &UiCommand::sig_triggered, this, [this]() {
        const QByteArray logContent = m_clientLineModel->toPlainText().toUtf8();
        const QString newFileName = "log-"
                                    + QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss")
                                    + ".txt";
        QFileDialog::saveFileContent(logContent, newFileName);
    });
    connect(m_commandRegistry->command("client.save-log-html"),
            &UiCommand::sig_triggered,
            this,
            [this]() {
                const QByteArray logContent = m_clientLineModel->toHtml().toUtf8();
                const QString newFileNameHtml = "log-"
                                                + QDateTime::currentDateTime().toString(
                                                    "yyyyMMdd-hhmmss")
                                                + ".html";
                QFileDialog::saveFileContent(logContent, newFileNameHtml);
            });

    // room.create -- mirrors MainWindow::slot_onCreateRoom() ->
    // MapCanvas::slot_createRoom(); MapCanvasCore owns the exact same slot.
    connect(m_commandRegistry->command("room.create"),
            &UiCommand::sig_triggered,
            m_mapCanvasCore,
            &MapCanvasCore::slot_createRoom);

    // Registry-side selection groups: purely organizational (no exclusivity,
    // mirrors mainwindow.cpp's createActions() `addToGroup(..., false)`
    // calls), used by wireSelectionCommands() below to enable/disable the
    // whole cluster together as the canvas selection changes. Group members
    // start disabled, mirroring MainWindow::createActions()'s
    // selectedRoomActGroup/selectedConnectionActGroup/infomarkGroup->
    // setEnabled(false).
    for (const char *const id : {"room.delete-selected",
                                 "room.move-up-selected",
                                 "room.move-down-selected",
                                 "room.merge-up-selected",
                                 "room.merge-down-selected",
                                 "room.connect-to-neighbours"}) {
        m_commandRegistry->addToGroup(m_commandRegistry->command(id), "room.selection", false);
    }
    m_commandRegistry->addToGroup(m_commandRegistry->command("connection.delete-selected"),
                                  "connection.selection",
                                  false);
    m_commandRegistry->addToGroup(m_commandRegistry->command("infomark.delete-selected"),
                                  "infomark.selection",
                                  false);
    m_commandRegistry->setGroupEnabled("room.selection", false);
    m_commandRegistry->setGroupEnabled("connection.selection", false);
    m_commandRegistry->setGroupEnabled("infomark.selection", false);
}

void QmlShellWindow::wireDialogCommands()
{
    // help.about -- mirrors MainWindow::slot_about()'s MMAPPER_WITH_QML
    // branch exactly: a fresh AboutInfo + QmlDialog per open, deleted on
    // close. Reuses the existing QmlDialog host (see this commit's task
    // report for why QmlDialog-hosted dialogs work unchanged under Shell
    // B's GL-backed scene graph).
    connect(m_commandRegistry->command("help.about"), &UiCommand::sig_triggered, this, [this]() {
        auto *const dialog = new QmlDialog(tr("About MMapper"), "AboutDialog", nullptr);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        auto *const info = new AboutInfo(dialog);
        dialog->setContextProperty("aboutInfo", info);
        dialog->setQmlSource(QUrl(QStringLiteral("qrc:/qt/qml/MMapper/AboutDialog.qml")));
        dialog->open();
    });

    // help.about-qt -- mirrors mainwindow.cpp's aboutQtAct wiring
    // (`connect(aboutQtAct, &QAction::triggered, qApp, &QApplication::aboutQt)`).
    connect(m_commandRegistry->command("help.about-qt"),
            &UiCommand::sig_triggered,
            qApp,
            &QApplication::aboutQt);

    // help.website/forum/wiki/setup/newbie/report-issue/vote -- mirror
    // mainwindow.cpp's slot_openMumeWebsite()/slot_openMumeForum()/
    // slot_openMumeWiki()/slot_openSettingUpMmapper()/slot_openNewbieHelp()/
    // onReportIssueTriggered()/slot_voteForMUME() (mainwindow.cpp:2895-2941),
    // each just a QDesktopServices::openUrl() one-liner.
    connect(m_commandRegistry->command("help.website"), &UiCommand::sig_triggered, this, []() {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://mume.org/")));
    });
    connect(m_commandRegistry->command("help.forum"), &UiCommand::sig_triggered, this, []() {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://mume.org/forum/")));
    });
    connect(m_commandRegistry->command("help.wiki"), &UiCommand::sig_triggered, this, []() {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://mume.org/wiki/")));
    });
    connect(m_commandRegistry->command("help.setup"), &UiCommand::sig_triggered, this, []() {
        QDesktopServices::openUrl(
            QUrl(QStringLiteral("https://github.com/MUME/MMapper/wiki/Troubleshooting")));
    });
    connect(m_commandRegistry->command("help.report-issue"), &UiCommand::sig_triggered, this, []() {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/MUME/MMapper/issues")));
    });
    connect(m_commandRegistry->command("help.newbie"), &UiCommand::sig_triggered, this, []() {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://mume.org/newbie.php")));
    });
    connect(m_commandRegistry->command("help.vote"), &UiCommand::sig_triggered, this, []() {
        QDesktopServices::openUrl(QUrl(QStringLiteral(
            "https://www.mudconnect.com/cgi-bin/search.cgi?mode=mud_listing&mud=MUME+-+Multi+"
            "Users+In+Middle+Earth")));
    });

    // help.check-for-update -- mirrors MainWindow's constant-if branch in
    // its ctor: a single persistent UpdateChecker + QmlDialog, raised
    // whenever a newer version is found or the menu item is triggered. Like
    // mainwindow.cpp, NO_UPDATER builds never construct the checker; the
    // registered command stays disabled there.
    if constexpr (!NO_UPDATER) {
        m_updateChecker = new UpdateChecker(this);
        {
            auto *const dialog = new QmlDialog(tr("MMapper Updater"), "UpdateDialog", nullptr);
            dialog->setContextProperty("updateChecker", m_updateChecker);
            dialog->setQmlSource(QUrl(QStringLiteral("qrc:/qt/qml/MMapper/UpdateDialog.qml")));
            m_updateDialog = dialog;
        }
        connect(m_updateChecker, &UpdateChecker::sig_showDialog, this, [this]() {
            if (m_updateDialog.isNull()) {
                return;
            }
            m_updateDialog->show();
            m_updateDialog->raise();
            m_updateDialog->activateWindow();
        });
        connect(m_commandRegistry->command("help.check-for-update"),
                &UiCommand::sig_triggered,
                this,
                [this]() { m_updateChecker->check(); });
    } else {
        m_commandRegistry->command("help.check-for-update")->setEnabled(false);
    }

    // room.find -- mirrors mainwindow.cpp's Find Room Dialog construction
    // block plus its "Find Room Dialog Connections" wiring. sig_center
    // connects directly to MapCanvasCore::slot_setScroll() instead of
    // MapWindow::slot_centerOnWorldPos() (this shell has no MapWindow/
    // scrollbar widgets -- see MapWindow::slot_centerOnWorldPos(), which
    // itself bottoms out in the same slot_setScroll() call after updating
    // scrollbar widgets this shell doesn't have). sig_log connects to
    // LogModel::append(), whose (mod, message) signature matches
    // sig_log(QString, QString) exactly. sig_editSelection reuses this
    // class's own room-edit-dialog opening lambda (see
    // wireSelectionCommands() below) via the room.edit-selected command's
    // trigger, mirroring MainWindow::slot_onEditRoomSelection() being
    // connected directly there.
    m_findRoomsController = new FindRoomsController(deref(m_mapData), this);
    {
        auto *const dialog = new QmlDialog(tr("Find Rooms"), "FindRoomsDlg", nullptr);
        dialog->setContextProperty("findRoomsController", m_findRoomsController);
        dialog->setContextProperty("findRoomsModel", m_findRoomsController->getModel());
        dialog->setQmlSource(QUrl(QStringLiteral("qrc:/qt/qml/MMapper/FindRoomsDialog.qml")));
        dialog->restoreGeometry(getConfig().findRoomsDialog.geometry);
        m_findRoomsDialog = dialog;
    }
    connect(m_findRoomsController,
            &FindRoomsController::sig_newRoomSelection,
            m_mapCanvasCore,
            &MapCanvasCore::slot_setRoomSelection);
    connect(m_findRoomsController,
            &FindRoomsController::sig_center,
            m_mapCanvasCore,
            &MapCanvasCore::slot_setScroll);
    connect(m_findRoomsController, &FindRoomsController::sig_log, m_logModel, &LogModel::append);
    connect(m_findRoomsController, &FindRoomsController::sig_editSelection, this, [this]() {
        if (UiCommand *const cmd = m_commandRegistry->command("room.edit-selected")) {
            cmd->trigger();
        }
    });
    connect(m_findRoomsDialog, &QDialog::finished, this, [this](MAYBE_UNUSED int result) {
        if (!m_findRoomsDialog.isNull()) {
            setConfig().findRoomsDialog.geometry = m_findRoomsDialog->saveGeometry();
        }
    });
    connect(m_commandRegistry->command("room.find"), &UiCommand::sig_triggered, this, [this]() {
        if (m_findRoomsDialog.isNull()) {
            return;
        }
        m_findRoomsDialog->show();
        m_findRoomsDialog->raise();
        m_findRoomsDialog->activateWindow();
    });

    // edit.preferences -- mirrors MainWindow::slot_onPreferences()'s
    // MMAPPER_WITH_QML branch: a single persistent PreferencesController +
    // PreferencesQmlDialog, built lazily on first trigger. dialogParent is
    // nullptr here (QmlShellWindow has no QWidget of its own -- see this
    // commit's task report), so the color-picker/password-manager dialogs
    // AnsiColorPickerLauncher/PasswordDialogLauncher construct won't have a
    // parent widget to center over; they still function correctly
    // standalone (documented deviation, see the task report).
    connect(m_commandRegistry->command("edit.preferences"), &UiCommand::sig_triggered, this, [this]() {
        if (m_preferencesDialog.isNull()) {
            auto *const controller = new PreferencesController(nullptr, this);
            auto *const dialog = new PreferencesQmlDialog(tr("Preferences"),
                                                          "ConfigDialog",
                                                          nullptr);
            dialog->setContextProperty("preferencesController", controller);
            dialog->setQmlSource(QUrl(QStringLiteral("qrc:/qt/qml/MMapper/PreferencesDialog.qml")));

            // See ParserPageAdapter.h's ColorPicker doc comment for
            // why this can't be wired up inside ParserPageAdapter.cpp
            // itself.
            deref(controller->getParser())
                .setColorPicker([](const QString &ansiString,
                                   QWidget *const colorDialogParent,
                                   std::function<void(QString)> callback) {
                    ansi_color_picker::getColor(ansiString, colorDialogParent, std::move(callback));
                });

            // See GeneralPageAdapter.h's PasswordDialogLauncher doc
            // comment for why this can't be wired up inside
            // GeneralPageAdapter.cpp itself.
            deref(controller->getGeneral())
                .setPasswordDialogLauncher(
                    [](const QString &accountName,
                       const QString &password,
                       bool hasStoredPassword,
                       QWidget *const dialogParent,
                       std::function<void(const QString &, const QString &)> onAccepted,
                       std::function<void()> onDeleteRequested) {
                        password_dialog::manage(accountName,
                                                password,
                                                hasStoredPassword,
                                                dialogParent,
                                                std::move(onAccepted),
                                                std::move(onDeleteRequested));
                    });

            // Graphics settings changes apply directly to
            // MapCanvasCore here (this shell has no MapWindow to
            // relay through -- MainWindow::MapWindow::
            // slot_graphicsSettingsChanged() itself just forwards to
            // the canvas).
            connect(controller,
                    &PreferencesController::sig_graphicsSettingsChanged,
                    m_mapCanvasCore,
                    &MapCanvasCore::graphicsSettingsChanged);
            connect(controller,
                    &PreferencesController::sig_groupSettingsChanged,
                    m_groupManager,
                    &Mmapper2Group::slot_groupSettingsChanged);
            // GroupManagerSettings has no ChangeMonitor (see
            // QmlConfig.h), so QmlConfig cannot observe the
            // controller's writes on its own; re-sync it explicitly
            // whenever the dialog reports a group settings change or
            // closes (mirrors mainwindow.cpp's equivalent wiring).
            connect(controller,
                    &PreferencesController::sig_groupSettingsChanged,
                    m_qmlConfig,
                    &QmlConfig::reload);
            connect(dialog, &QDialog::finished, this, [this](MAYBE_UNUSED int result) {
                m_qmlConfig->reload();
                deref(m_descriptionAdapter).reloadConfig();
            });

            m_preferencesController = controller;
            m_preferencesDialog = dialog;
        }

        deref(m_preferencesController).reloadAll();

        auto &dialog = deref(m_preferencesDialog);
        dialog.show();
        dialog.raise();
        dialog.activateWindow();
    });
}

void QmlShellWindow::wireSelectionCommands()
{
    // room.edit-selected/infomark.edit-selected/room.goto-selected/
    // room.force-update-selected are registered live (see isLiveCommand())
    // but start with no selection to act on. room.delete-selected/move-*/
    // merge-*/connect-to-neighbours and connection.delete-selected already
    // start disabled via their "room.selection"/"connection.selection"
    // groups (see registerCommands()); infomark.delete-selected likewise via
    // "infomark.selection".
    if (UiCommand *const cmd = m_commandRegistry->command("room.edit-selected")) {
        cmd->setEnabled(false);
    }
    if (UiCommand *const cmd = m_commandRegistry->command("infomark.edit-selected")) {
        cmd->setEnabled(false);
    }
    if (UiCommand *const cmd = m_commandRegistry->command("room.goto-selected")) {
        cmd->setEnabled(false);
    }
    if (UiCommand *const cmd = m_commandRegistry->command("room.force-update-selected")) {
        cmd->setEnabled(false);
    }

    // room.edit-selected is a member of AppCore::onNewRoomSelection()'s
    // "room.selection" group (see registerCommands()'s
    // addToGroup(..., "room.selection", false) loop, which mirrors
    // mainwindow.cpp's identical loop -- room.edit-selected is included
    // there too), so add it to the group here rather than managing its
    // enabled state by hand alongside the other room.selection members.
    m_commandRegistry->addToGroup(m_commandRegistry->command("room.edit-selected"),
                                  "room.selection",
                                  false);
    // infomark.edit-selected is likewise a member of AppCore::
    // onNewInfomarkSelection()'s "infomark.selection" group alongside
    // infomark.delete-selected (mirrors mainwindow.cpp's
    // `for (id : {"infomark.delete-selected", "infomark.edit-selected"})`
    // loop), so its enabled state comes from the group rather than a
    // separate setEnabled() call below.
    m_commandRegistry->addToGroup(m_commandRegistry->command("infomark.edit-selected"),
                                  "infomark.selection",
                                  false);

    // Mirrors MainWindow::slot_newRoomSelection()/slot_newConnectionSelection()/
    // slot_newInfomarkSelection(): track the canvas's current selection here
    // (read by the room.edit-selected/infomark.edit-selected/room.create
    // triggered handlers and by MapContextMenu.qml's hasRoomSelection/
    // roomSelectionEmpty/hasConnectionSelection/hasInfomarkSelection
    // properties -- see QmlShellWindow.h's Q_PROPERTY block), delegating the
    // "what does this enable/disable" logic to AppCore (m_appCore), exactly
    // like MainWindow does.
    connect(m_mapCanvasCore,
            &MapCanvasCore::sig_newRoomSelection,
            this,
            [this](const SigRoomSelection &rs) {
                m_roomSelection = !rs.isValid() ? nullptr : rs.getShared();
                m_appCore->onNewRoomSelection(rs);
                emit sig_roomSelectionStateChanged();
            });
    connect(m_mapCanvasCore,
            &MapCanvasCore::sig_newConnectionSelection,
            this,
            [this](ConnectionSelection *const cs) {
                m_connectionSelection = (cs != nullptr) ? cs->shared_from_this() : nullptr;
                m_appCore->onNewConnectionSelection(m_connectionSelection != nullptr);
                emit sig_connectionSelectionStateChanged();
            });
    connect(m_mapCanvasCore,
            &MapCanvasCore::sig_newInfomarkSelection,
            this,
            [this](InfomarkSelection *const is) {
                const bool isNonNull = is != nullptr;
                m_infoMarkSelection = isNonNull ? is->shared_from_this() : nullptr;
                const bool openEditor = m_appCore->onNewInfomarkSelection(isNonNull,
                                                                          isNonNull ? is->size()
                                                                                    : size_t{0});
                emit sig_infomarkSelectionStateChanged();
                if (openEditor) {
                    // Mirrors MainWindow::slot_newInfomarkSelection(): an
                    // empty (just-drawn) infomark selection immediately opens
                    // the editor to create a new marker.
                    if (UiCommand *const cmd = m_commandRegistry->command(
                            "infomark.edit-selected")) {
                        cmd->trigger();
                    }
                }
            });

    // Mirrors MainWindow::wireConnections()'s
    // `connect(canvas, &MapCanvas::sig_selectionChanged, ...)` lambda:
    // re-enables the move/merge up/down room commands based on whether an
    // adjacent-layer room exists under the current selection.
    connect(m_mapCanvasCore, &MapCanvasCore::sig_selectionChanged, this, [this]() {
        m_appCore->updateRoomOffsetCommands(m_roomSelection);
    });

    // Mirrors MapWindow::slot_showTooltip(): MapCanvasCore emits
    // sig_showTooltip() when a room is left-clicked in MOVE mode without a
    // drag, carrying the room-details preview text. The widget shell routes
    // it through MapWindow to a QToolTip anchored on the canvas container;
    // there is no such container here (the canvas is a QQuickItem in a
    // QQuickWindow), so anchor at the cursor -- which, for a click-triggered
    // tooltip, is exactly the point that was tapped. This is the same
    // no-hosting-widget fallback ClockAdapter uses (see the setToolTipWidget
    // comment in this window's constructor).
    connect(m_mapCanvasCore,
            &MapCanvasCore::sig_showTooltip,
            this,
            [](const QString &text, const QPoint &) { QToolTip::showText(QCursor::pos(), text); });

    // room.edit-selected -- mirrors MainWindow::slot_onEditRoomSelection()'s
    // MMAPPER_WITH_QML branch: a single persistent RoomEditController +
    // RoomEditQmlDialog per selection-edit session, raised instead of
    // duplicated if triggered again while already open.
    connect(m_commandRegistry->command("room.edit-selected"),
            &UiCommand::sig_triggered,
            this,
            [this]() {
                if (m_roomSelection == nullptr) {
                    return;
                }
                if (!m_roomEditDialog.isNull()) {
                    m_roomEditDialog->setFocus();
                    m_roomEditDialog->raise();
                    m_roomEditDialog->activateWindow();
                    return;
                }

                auto *const controller = new RoomEditController(nullptr);
                auto *const dialog = new RoomEditQmlDialog(controller,
                                                           tr("Room properties"),
                                                           "RoomEditAttrDlg",
                                                           nullptr);
                controller->setParent(dialog);
                controller->setRoomSelection(m_roomSelection, m_mapData);
                connect(controller,
                        &RoomEditController::sig_requestUpdate,
                        m_mapCanvasCore,
                        &MapCanvasCore::slot_requestUpdate);
                connect(controller,
                        &RoomEditController::sig_error,
                        this,
                        [this](const QString &message) {
                            deref(m_engine->rootContext()).setContextProperty("statusText", message);
                        });

                dialog->setContextProperty("roomEditController", controller);
                dialog->setQmlSource(QUrl(QStringLiteral("qrc:/qt/qml/MMapper/RoomEditDialog.qml")));
                dialog->restoreGeometry(getConfig().roomEditDialog.geometry);

                m_roomEditController = controller;
                m_roomEditDialog = dialog;

                dialog->show();
                connect(dialog, &QDialog::finished, this, [this](MAYBE_UNUSED int result) {
                    if (!m_roomEditDialog.isNull()) {
                        setConfig().roomEditDialog.geometry = m_roomEditDialog->saveGeometry();
                    }
                    m_roomEditController = nullptr;
                });
            });

    // infomark.edit-selected -- mirrors MainWindow::slot_onEditInfomarkSelection()'s
    // MMAPPER_WITH_QML branch: a fresh InfomarkEditController + QmlDialog
    // per open (unlike room.edit-selected, MainWindow never de-duplicated
    // this one either).
    connect(m_commandRegistry->command("infomark.edit-selected"),
            &UiCommand::sig_triggered,
            this,
            [this]() {
                if (m_infoMarkSelection == nullptr) {
                    return;
                }

                auto *const controller = new InfomarkEditController(nullptr);
                auto *const dialog = new QmlDialog(tr("Edit Markers"), "InfomarksEditDlg", nullptr);
                controller->setParent(dialog);
                controller->setInfomarkSelection(m_infoMarkSelection, m_mapData);
                dialog->setContextProperty("infomarkEditController", controller);
                dialog->setQmlSource(
                    QUrl(QStringLiteral("qrc:/qt/qml/MMapper/InfomarkEditDialog.qml")));
                dialog->setAttribute(Qt::WA_DeleteOnClose);
                dialog->restoreGeometry(getConfig().infomarksDialog.geometry);
                connect(dialog, &QDialog::finished, dialog, [dialog](MAYBE_UNUSED int result) {
                    setConfig().infomarksDialog.geometry = dialog->saveGeometry();
                });
                dialog->show();
            });

    // room.goto-selected -- mirrors MainWindow's gotoRoomAct trigger lambda
    // (mainwindow.cpp).
    connect(m_commandRegistry->command("room.goto-selected"),
            &UiCommand::sig_triggered,
            this,
            [this]() {
                if (m_roomSelection == nullptr) {
                    return;
                }
                auto &mapData = deref(m_mapData);
                auto &sel = deref(m_roomSelection);
                sel.removeMissing(mapData);
                if (m_roomSelection->size() == 1) {
                    const RoomId id = m_roomSelection->getFirstRoomId();
                    m_mapData->setRoom(id);
                }
            });

    // room.force-update-selected -- mirrors MainWindow::slot_forceMapperToRoom().
    connect(m_commandRegistry->command("room.force-update-selected"),
            &UiCommand::sig_triggered,
            this,
            [this]() {
                if (m_roomSelection == nullptr) {
                    return;
                }
                auto &mapData = deref(m_mapData);
                auto &sel = deref(m_roomSelection);
                sel.removeMissing(mapData);
                if (m_roomSelection->size() == 1) {
                    const RoomId id = m_roomSelection->getFirstRoomId();
                    m_mapData->setRoom(id);
                    m_pathMachine->forceUpdate(id);
                }
            });

    // room.delete-selected -- mirrors MainWindow::slot_onDeleteRoomSelection():
    // MapData::applyChangesToList() (multi-room aware -- one Change per room
    // in the selection) followed by clearing the canvas's room selection.
    connect(m_commandRegistry->command("room.delete-selected"),
            &UiCommand::sig_triggered,
            this,
            [this]() {
                if (m_roomSelection == nullptr) {
                    return;
                }
                m_mapData->applyChangesToList(deref(m_roomSelection),
                                              [](const RawRoom &room) -> Change {
                                                  return Change{
                                                      room_change_types::RemoveRoom{room.getId()}};
                                              });
                m_mapCanvasCore->slot_clearRoomSelection();
            });

    // room.move-up-selected/room.move-down-selected -- mirrors
    // MainWindow::slot_onMoveUpRoomSelection()/slot_onMoveDownRoomSelection()
    // (via slot_moveRoomSelection()): MoveRelative by one Z layer, then
    // follow the selection to the new layer (mirrors slot_onLayerUp()/
    // slot_onLayerDown() -> AppCore::layerUp()/layerDown() ->
    // MapCanvas::slot_layerUp()/slot_layerDown(), which MapCanvasCore
    // implements identically).
    connect(m_commandRegistry->command("room.move-up-selected"),
            &UiCommand::sig_triggered,
            this,
            [this]() {
                if (m_roomSelection == nullptr) {
                    return;
                }
                const Coordinate offset(0, 0, 1);
                m_mapData->applyChangesToList(deref(m_roomSelection),
                                              [&offset](const RawRoom &room) -> Change {
                                                  return Change{
                                                      room_change_types::MoveRelative{room.getId(),
                                                                                      offset}};
                                              });
                m_mapCanvasCore->slot_layerUp();
            });
    connect(m_commandRegistry->command("room.move-down-selected"),
            &UiCommand::sig_triggered,
            this,
            [this]() {
                if (m_roomSelection == nullptr) {
                    return;
                }
                const Coordinate offset(0, 0, -1);
                m_mapData->applyChangesToList(deref(m_roomSelection),
                                              [&offset](const RawRoom &room) -> Change {
                                                  return Change{
                                                      room_change_types::MoveRelative{room.getId(),
                                                                                      offset}};
                                              });
                m_mapCanvasCore->slot_layerDown();
            });

    // room.merge-up-selected/room.merge-down-selected -- mirrors
    // MainWindow::slot_onMergeUpRoomSelection()/slot_onMergeDownRoomSelection()
    // (via slot_mergeRoomSelection()): MergeRelative by one Z layer, follow
    // the selection to the new layer, then switch back to room-select mouse
    // mode (mirrors slot_onModeRoomSelect() -> AppCore::setCanvasMouseMode();
    // this shell's mouse-mode commands are wired directly to MapCanvasCore --
    // see wireMouseModeCommand() -- so the mouse-mode.room-select command is
    // triggered here instead, keeping its checked state in sync too).
    connect(m_commandRegistry->command("room.merge-up-selected"),
            &UiCommand::sig_triggered,
            this,
            [this]() {
                if (m_roomSelection == nullptr) {
                    return;
                }
                const Coordinate offset(0, 0, 1);
                m_mapData->applyChangesToList(deref(m_roomSelection),
                                              [&offset](const RawRoom &room) -> Change {
                                                  return Change{
                                                      room_change_types::MergeRelative{room.getId(),
                                                                                       offset}};
                                              });
                m_mapCanvasCore->slot_layerUp();
                if (UiCommand *const cmd = m_commandRegistry->command("mouse-mode.room-select")) {
                    cmd->trigger();
                }
            });
    connect(m_commandRegistry->command("room.merge-down-selected"),
            &UiCommand::sig_triggered,
            this,
            [this]() {
                if (m_roomSelection == nullptr) {
                    return;
                }
                const Coordinate offset(0, 0, -1);
                m_mapData->applyChangesToList(deref(m_roomSelection),
                                              [&offset](const RawRoom &room) -> Change {
                                                  return Change{
                                                      room_change_types::MergeRelative{room.getId(),
                                                                                       offset}};
                                              });
                m_mapCanvasCore->slot_layerDown();
                if (UiCommand *const cmd = m_commandRegistry->command("mouse-mode.room-select")) {
                    cmd->trigger();
                }
            });

    // room.connect-to-neighbours -- mirrors
    // MainWindow::slot_onConnectToNeighboursRoomSelection(): connects every
    // room in the selection to its map neighbours via Changes.h's
    // connectToNeighbors(), batched into a single ChangeList.
    connect(m_commandRegistry->command("room.connect-to-neighbours"),
            &UiCommand::sig_triggered,
            this,
            [this]() {
                if (m_roomSelection == nullptr) {
                    return;
                }
                auto &mapData = deref(m_mapData);
                auto &sel = deref(m_roomSelection);
                sel.removeMissing(mapData);

                const Map &map = mapData.getCurrentMap();
                ChangeList changes;
                for (const RoomId id : sel) {
                    const auto &room = map.getRoomHandle(id);
                    connectToNeighbors(changes, room, ConnectToNeighborsArgs{});
                }
                if (changes.getChanges().empty()) {
                    return;
                }
                mapData.applyChanges(changes);
            });

    // connection.delete-selected -- mirrors
    // MainWindow::slot_onDeleteConnectionSelection().
    connect(m_commandRegistry->command("connection.delete-selected"),
            &UiCommand::sig_triggered,
            this,
            [this]() {
                if (m_connectionSelection == nullptr) {
                    return;
                }
                const auto &first = m_connectionSelection->getFirst();
                const auto &second = m_connectionSelection->getSecond();
                const auto &r1 = first.room;
                const auto &r2 = second.room;
                if (!r1 || !r2) {
                    return;
                }
                const ExitDirEnum dir1 = first.direction;
                const RoomId &id1 = r1.getId();
                const RoomId &id2 = r2.getId();

                m_mapCanvasCore->slot_clearConnectionSelection();

                m_mapData->applySingleChange(Change{exit_change_types::ModifyExitConnection{
                    ChangeTypeEnum::Remove, id1, dir1, id2, WaysEnum::TwoWay}});
            });

    // infomark.delete-selected -- mirrors
    // MainWindow::slot_onDeleteInfomarkSelection().
    connect(m_commandRegistry->command("infomark.delete-selected"),
            &UiCommand::sig_triggered,
            this,
            [this]() {
                if (m_infoMarkSelection == nullptr) {
                    return;
                }
                {
                    const auto tmp = std::exchange(m_infoMarkSelection, nullptr);
                    ChangeList changes;
                    for (const InfomarkId id : tmp->getMarkerList()) {
                        changes.add(Change{infomark_change_types::RemoveInfomark{id}});
                    }
                    if (!changes.empty()) {
                        m_mapData->applyChanges(changes);
                    }
                }
                m_mapCanvasCore->slot_clearInfomarkSelection();
            });
}

void QmlShellWindow::wireFileCommands()
{
    // file.save starts disabled: mirrors MainWindow's saveAct, which stays
    // disabled until MainWindow::setMapModified(true) enables it (see
    // mainwindow.cpp's wireConnections()/onSuccessfulLoad()) -- there is
    // nothing to save until a map is loaded or edited. file.open/file.new/
    // file.merge/file.save-as have no such precondition, and were left
    // enabled by registerCommands() (see isLiveCommand()).
    if (UiCommand *const cmd = m_commandRegistry->command("file.save")) {
        cmd->setEnabled(false);
    }

    // Mirrors MainWindow::wireConnections()'s
    // `connect(m_mapData, &MapData::sig_onDataChanged, ...)`: the "map was
    // edited" splash-hide/save-enable/title-refresh trigger (see
    // updateMapModifiedState()'s doc comment).
    connect(m_mapData, &MapData::sig_onDataChanged, this, &QmlShellWindow::updateMapModifiedState);

    connect(m_commandRegistry->command("file.new"), &UiCommand::sig_triggered, this, [this]() {
        // Mirrors MainWindow::slot_newFile().
        if (maybeSave()) {
            forceNewFile();
        }
    });

    connect(m_commandRegistry->command("file.open"), &UiCommand::sig_triggered, this, [this]() {
        // Mirrors MainWindow::slot_open()'s non-Wasm branch: same
        // maybeSave() guard, name filter, and lastMapDirectory config key
        // as the widget shell (see mainwindow.cpp), so both shells remember
        // the same "last opened" directory. This shell is desktop-only
        // (never constructed under Q_OS_WASM -- see QmlShellWindow.h's file
        // comment), so there is no QFileDialog::getOpenFileContent() branch
        // to mirror.
        if (!maybeSave()) {
            return;
        }
        const auto nameFilter = QStringLiteral(
            "MMapper2 maps (*.mm2)"
            ";;MMapper2 XML or Pandora maps (*.xml)"
            ";;Alternate suffix for MMapper2 XML maps (*.mm2xml)");
        const QString &savedLastMapDir = setConfig().autoLoad.lastMapDirectory;
        const QString fileName = QFileDialog::getOpenFileName(nullptr,
                                                              tr("Choose map file ..."),
                                                              savedLastMapDir,
                                                              nameFilter);
        if (fileName.isEmpty()) {
            return;
        }
        try {
            loadFile(MapSource::alloc(fileName, std::nullopt));
            setConfig().autoLoad.lastMapDirectory = QFileInfo(fileName).dir().absolutePath();
        } catch (const std::exception &ex) {
            deref(m_engine->rootContext())
                .setContextProperty("statusText",
                                    tr("Cannot open file %1:\n%2.").arg(fileName, ex.what()));
        }
    });

    connect(m_commandRegistry->command("file.merge"), &UiCommand::sig_triggered, this, [this]() {
        // Mirrors MainWindow::slot_merge()'s non-Wasm branch. No maybeSave()
        // guard here, matching MainWindow: merging adds to the existing map
        // rather than discarding it, so there is nothing to lose.
        const auto nameFilter = QStringLiteral(
            "MMapper2 maps (*.mm2)"
            ";;MMapper2 XML or Pandora maps (*.xml)"
            ";;Alternate suffix for MMapper2 XML maps (*.mm2xml)");
        const QString &savedLastMapDir = setConfig().autoLoad.lastMapDirectory;
        const QString fileName = QFileDialog::getOpenFileName(nullptr,
                                                              tr("Choose map file ..."),
                                                              savedLastMapDir,
                                                              nameFilter);
        if (fileName.isEmpty()) {
            return;
        }
        mergeFile(fileName);
    });

    connect(m_commandRegistry->command("file.reload"), &UiCommand::sig_triggered, this, [this]() {
        // Mirrors MainWindow::slot_reload(): re-opens MapData's current
        // filename (see loadFile()'s doc comment on current-file tracking --
        // MapData::getFileName() is the shell's only "current file" state,
        // same as MainWindow).
        if (!maybeSave()) {
            return;
        }
        const QString filename = m_mapData->getFileName();
        if (filename.isEmpty()) {
            deref(m_engine->rootContext())
                .setContextProperty("statusText", tr("No filename provided"));
            return;
        }
        try {
            loadFile(MapSource::alloc(filename, std::nullopt));
        } catch (const std::exception &ex) {
            deref(m_engine->rootContext())
                .setContextProperty("statusText",
                                    tr("Cannot open file %1:\n%2.").arg(filename, ex.what()));
        }
    });

    connect(m_commandRegistry->command("file.save"), &UiCommand::sig_triggered, this, [this]() {
        // Mirrors MainWindow::slot_save().
        std::ignore = doSave();
    });

    connect(m_commandRegistry->command("file.save-as"), &UiCommand::sig_triggered, this, [this]() {
        // Mirrors MainWindow::slot_saveAs().
        std::ignore = doSaveAs();
    });

    // The 4 file.export.* commands mirror mainwindow-saveslots.cpp's
    // slot_exportBaseMap()/slot_exportMm2xmlMap()/slot_exportWebMap()/
    // slot_exportMmpMap(): a plain QFileDialog (getSaveFileName(), or
    // getExistingDirectory() for the Web export -- see MapDestination's
    // isDirectory() handling) using the same suggested-name/filter logic,
    // routed through saveMapFile()'s async task + progress popup exactly
    // like file.save/file.save-as (exports are saves: AllowCancel::Forbid,
    // same as MainWindow's AsyncSaver -- see saveMapFile()).
    connect(m_commandRegistry->command("file.export.base-map"),
            &UiCommand::sig_triggered,
            this,
            [this]() {
                const QString suggestedName
                    = QFileInfo(m_mapData->getFileName()).baseName().append("-base.mm2");
                QDir lastMapDir;
                if (!lastMapDir.mkpath(getConfig().autoLoad.lastMapDirectory)) {
                    lastMapDir.setPath(QDir::homePath());
                } else {
                    lastMapDir.setPath(getConfig().autoLoad.lastMapDirectory);
                }
                const QString fileName = QFileDialog::getSaveFileName(nullptr,
                                                                      tr("Choose map file name ..."),
                                                                      lastMapDir.filePath(
                                                                          suggestedName),
                                                                      tr("MMapper maps (*.mm2)"));
                if (fileName.isEmpty()) {
                    deref(m_engine->rootContext())
                        .setContextProperty("statusText", tr("No filename provided"));
                    return;
                }
                saveMapFile(fileName, SaveModeEnum::BASEMAP, SaveFormatEnum::MM2);
            });

    connect(m_commandRegistry->command("file.export.mm2xml"),
            &UiCommand::sig_triggered,
            this,
            [this]() {
                const QString suggestedName
                    = QFileInfo(m_mapData->getFileName()).baseName().append(".xml");
                QDir lastMapDir;
                if (!lastMapDir.mkpath(getConfig().autoLoad.lastMapDirectory)) {
                    lastMapDir.setPath(QDir::homePath());
                } else {
                    lastMapDir.setPath(getConfig().autoLoad.lastMapDirectory);
                }
                const QString fileName
                    = QFileDialog::getSaveFileName(nullptr,
                                                   tr("Choose map file name ..."),
                                                   lastMapDir.filePath(suggestedName),
                                                   tr("MMapper2 XML maps (*.xml)"));
                if (fileName.isEmpty()) {
                    deref(m_engine->rootContext())
                        .setContextProperty("statusText", tr("No filename provided"));
                    return;
                }
                saveMapFile(fileName, SaveModeEnum::FULL, SaveFormatEnum::MM2XML);
            });

    connect(m_commandRegistry->command("file.export.web"), &UiCommand::sig_triggered, this, [this]() {
        QDir lastMapDir;
        if (!lastMapDir.mkpath(getConfig().autoLoad.lastMapDirectory)) {
            lastMapDir.setPath(QDir::homePath());
        } else {
            lastMapDir.setPath(getConfig().autoLoad.lastMapDirectory);
        }
        const QString dirName = QFileDialog::getExistingDirectory(nullptr,
                                                                  tr("Choose map file name ..."),
                                                                  lastMapDir.path());
        if (dirName.isEmpty()) {
            deref(m_engine->rootContext())
                .setContextProperty("statusText", tr("No directory name provided"));
            return;
        }
        saveMapFile(dirName, SaveModeEnum::BASEMAP, SaveFormatEnum::WEB);
    });

    connect(m_commandRegistry->command("file.export.mmp"), &UiCommand::sig_triggered, this, [this]() {
        const QString suggestedName
            = QFileInfo(m_mapData->getFileName()).baseName().append("-mmp.xml");
        QDir lastMapDir;
        if (!lastMapDir.mkpath(getConfig().autoLoad.lastMapDirectory)) {
            lastMapDir.setPath(QDir::homePath());
        } else {
            lastMapDir.setPath(getConfig().autoLoad.lastMapDirectory);
        }
        const QString fileName = QFileDialog::getSaveFileName(nullptr,
                                                              tr("Choose map file name ..."),
                                                              lastMapDir.filePath(suggestedName),
                                                              tr("MMP maps (*.xml)"));
        if (fileName.isEmpty()) {
            deref(m_engine->rootContext())
                .setContextProperty("statusText", tr("No filename provided"));
            return;
        }
        saveMapFile(fileName, SaveModeEnum::FULL, SaveFormatEnum::MMP);
    });
}

bool QmlShellWindow::maybeSave()
{
    // Mirrors mainwindow-saveslots.cpp's MainWindow::maybeSave() exactly
    // (same QMessageBox::Save/Discard/Cancel prompt, describeChanges() text,
    // and default/escape buttons), minus the `this` QWidget parent (this
    // class has none -- the dialog is parented to nullptr, same tradeoff
    // QmlShellWindow.cpp's other QMessageBox uses already make -- see
    // startServices()'s "Unable to start the server" box).
    auto &mapData = deref(m_mapData);
    if (!mapData.dataChanged()) {
        return true;
    }

    const QString changes = mmqt::toQStringUtf8(mapData.describeChanges());

    QMessageBox dlg(nullptr);
    dlg.setIcon(QMessageBox::Warning);
    dlg.setWindowTitle(tr("mmapper"));
    dlg.setText(tr("The current map has been modified:\n\n") + changes
                + tr("\nDo you want to save the changes?"));
    dlg.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    dlg.setDefaultButton(QMessageBox::Discard);
    dlg.setEscapeButton(QMessageBox::Cancel);
    const int ret = dlg.exec();
    if (ret == QMessageBox::Save) {
        return doSave();
    }
    return ret != QMessageBox::Cancel;
}

bool QmlShellWindow::getRoomSelectionEmpty() const
{
    return m_roomSelection == nullptr || m_roomSelection->empty();
}

bool QmlShellWindow::getInfomarkSelectionEmpty() const
{
    return m_infoMarkSelection == nullptr || m_infoMarkSelection->empty();
}

bool QmlShellWindow::confirmClose()
{
    // Re-entrancy guard: MainShell.qml's Window.onClosing can fire again
    // while maybeSave()'s nested QMessageBox::exec() is still on the stack
    // (e.g. a second close request delivered by the platform while the
    // prompt has focus) -- see QmlShellWindow.h's m_confirmingClose comment.
    if (m_confirmingClose) {
        return false;
    }
    // Reduced subset of MainWindow::closeEvent()'s "ignore close while a
    // non-cancelable async task is running" rule: MainWindow tries to
    // cancel a cancelable task and wait, or hides itself and defers the
    // close until a non-cancelable save finishes (see
    // MainWindow::asyncTaskEnded()); this shell just refuses to close
    // outright while ANY IO task (load/merge/save) is in flight, cancelable
    // or not -- documented deviation, see the task report.
    if (m_ioInProgress) {
        deref(m_engine->rootContext())
            .setContextProperty("statusText", tr("Cannot quit while an IO task is in progress."));
        return false;
    }
    m_confirmingClose = true;
    const bool ok = maybeSave();
    m_confirmingClose = false;
    return ok;
}

bool QmlShellWindow::doSave()
{
    // Mirrors MainWindow::slot_save(): like MainWindow, this does NOT wait
    // for an already-started save to finish before returning true -- the
    // return value only reflects whether a save was (or a save-as dialog
    // successfully) started.
    const QString &fileName = m_mapData->getFileName();
    if (fileName.isEmpty() || m_mapData->isFileReadOnly()) {
        return doSaveAs();
    }
    saveMapFile(fileName, SaveModeEnum::FULL, SaveFormatEnum::MM2);
    return true;
}

bool QmlShellWindow::doSaveAs()
{
    // Mirrors MainWindow::slot_saveAs(), using a plain
    // QFileDialog::getSaveFileName() instead of mainwindow-saveslots.cpp's
    // createDefaultSaveDialog() helper (same filter/suggested-name logic,
    // just without that file's reusable multi-format dialog builders).
    QString suggestedName = m_mapData->getFileName();
    const QFileInfo currentFile(suggestedName);
    if (currentFile.exists()) {
        suggestedName = currentFile.suffix().contains("xml")
                            ? currentFile.baseName().append("-import.mm2")
                            : currentFile.baseName().append("-copy.mm2");
    }
    QDir lastMapDir;
    if (!lastMapDir.mkpath(getConfig().autoLoad.lastMapDirectory)) {
        lastMapDir.setPath(QDir::homePath());
    } else {
        lastMapDir.setPath(getConfig().autoLoad.lastMapDirectory);
    }
    const QString fileName = QFileDialog::getSaveFileName(nullptr,
                                                          tr("Choose map file name ..."),
                                                          lastMapDir.filePath(suggestedName),
                                                          tr("MMapper maps (*.mm2)"));
    if (fileName.isEmpty()) {
        deref(m_engine->rootContext()).setContextProperty("statusText", tr("No filename provided"));
        return false;
    }
    saveMapFile(fileName, SaveModeEnum::FULL, SaveFormatEnum::MM2);
    return true;
}

void QmlShellWindow::forceNewFile()
{
    // Mirrors MainWindow::forceNewFile(): discards the current map
    // unconditionally. Shared by file.new's guarded handler above and
    // loadFile() below (which discards the old map the same way right
    // before starting a load, mirroring MainWindow::loadFile()'s own
    // forceNewFile() call).
    auto &mapData = deref(m_mapData);
    mapData.clear();
    mapData.setFileName(QString{}, false);
    mapData.forcePosition(Coordinate{});

    updateWindowTitle();
    m_mapCanvasCore->slot_dataLoaded();
    m_groupController->slot_mapLoaded();
    m_descriptionAdapter->updateRoom(RoomHandle{});
    m_audioManager->onAreaChanged(RoomArea{});
}

void QmlShellWindow::loadFile(std::shared_ptr<MapSource> source)
{
    auto &src = deref(source);

    if (m_ioInProgress) {
        deref(m_engine->rootContext())
            .setContextProperty("statusText", tr("IO Task already in progress"));
        return;
    }
    if (src.getFileName().isEmpty()) {
        deref(m_engine->rootContext()).setContextProperty("statusText", tr("No filename provided"));
        return;
    }

    std::unique_ptr<AbstractMapStorage> pStorage;
    try {
        pStorage = maploadhelper::detectAndCreateStorage(source, this);
    } catch (const std::exception &ex) {
        deref(m_engine->rootContext())
            .setContextProperty("statusText",
                                tr("Cannot open file %1:\n%2.").arg(src.getFileName(), ex.what()));
        return;
    }
    connect(pStorage.get(), &AbstractMapStorage::sig_log, m_logModel, &LogModel::append);

    // Immediately discard the old map, mirroring
    // MainWindow::loadFile()'s forceNewFile() call.
    forceNewFile();

    m_ioInProgress = true;

    struct NODISCARD LoadState final
    {
        std::shared_ptr<AbstractMapStorage> storage;
        std::promise<std::optional<MapLoadData>> promise;
        std::future<std::optional<MapLoadData>> future = promise.get_future();
    };
    auto state = std::make_shared<LoadState>();
    state->storage = std::shared_ptr<AbstractMapStorage>(std::move(pStorage));
    const QString fileName = src.getFileName();

    const auto handle = async_tasks::startAsyncTask2(
        AsyncTaskTypeEnum::IO,
        AllowCancelEnum::Allow,
        "Map Loader",
        [state](const std::shared_ptr<ProgressCounter> &pc) {
            // background thread
            state->storage->setProgressCounter(pc);
            try {
                state->promise.set_value(maploadhelper::loadMapData(*state->storage));
            } catch (...) {
                state->promise.set_exception(std::current_exception());
            }
        },
        [this, state, fileName](const std::shared_ptr<ProgressCounter> &pc) {
            // main thread
            m_ioInProgress = false;
            endIoTaskProgress();
            const bool wasCanceled = deref(pc).hasRequestedCancel();
            std::optional<MapLoadData> result;
            try {
                result = state->future.get();
            } catch (const std::exception &ex) {
                deref(m_engine->rootContext())
                    .setContextProperty("statusText",
                                        tr("Cannot open file %1:\n%2.").arg(fileName, ex.what()));
            }

            // Mirrors mainwindow/utils.cpp's CanvasDisabler destructor: the
            // splash hides after any load attempt completes, success or
            // failure (see hideSplash()'s doc comment).
            hideSplash();

            if (wasCanceled || !result || result->mapPair.modified.getRoomsCount() == 0) {
                deref(m_engine->rootContext())
                    .setContextProperty("statusText",
                                        wasCanceled ? tr("Load canceled")
                                                    : tr("Failed to load file %1.").arg(fileName));
                return;
            }
            onSuccessfulLoad(*result);
        });
    // Progress popup, mirroring MainWindow::AsyncIO's QProgressDialog (see
    // beginIoTaskProgress()'s doc comment); the Tasks panel/tasksModel
    // remains a secondary view of the same async_tasks task this starts.
    beginIoTaskProgress(handle, tr("Loading map..."), /*cancelable=*/true);
}

void QmlShellWindow::onSuccessfulLoad(const MapLoadData &mapLoadData)
{
    // Mirrors MainWindow::onSuccessfulLoad() (mainwindow.cpp), minus:
    //  - statusBar() forwarding -- replaced by the statusText context
    //    property, same as the rest of this class;
    //  - mapChanged()/canvas->slot_mapChanged() -- MapCanvasCore has no
    //    equivalent slot to MapCanvas's widget-facing one; slot_dataLoaded()
    //    below already triggers the mesh rebuild that matters here.
    auto &mapData = deref(m_mapData);
    mapData.setMapData(mapLoadData);
    mapData.checkSize();

    m_mapCanvasCore->slot_dataLoaded();
    m_groupController->slot_mapLoaded();
    m_pathMachine->onMapLoaded();
    if (const auto room = mapData.getCurrentRoom()) {
        m_descriptionAdapter->updateRoom(room);
        m_audioManager->onAreaChanged(room.getArea());
    }

    updateMapModifiedState();
    deref(m_engine->rootContext()).setContextProperty("statusText", tr("File loaded"));
}

void QmlShellWindow::mergeFile(const QString &fileName)
{
    // Mirrors MainWindow::slot_merge()'s per-file body, using
    // maploadhelper::mergeMapData() (see MapLoadHelper.h/.cpp) -- extracted
    // from mainwindow-async.cpp's former background::merge_map_data() so
    // this shell's merge path can share it exactly like loadFile() already
    // shares maploadhelper::loadMapData(), rather than duplicating the
    // load-then-MapData::mergeMapData() logic here.
    if (m_ioInProgress) {
        deref(m_engine->rootContext())
            .setContextProperty("statusText", tr("IO Task already in progress"));
        return;
    }
    if (fileName.isEmpty()) {
        deref(m_engine->rootContext()).setContextProperty("statusText", tr("No filename provided"));
        return;
    }

    std::unique_ptr<AbstractMapStorage> pStorage;
    std::shared_ptr<MapSource> source;
    try {
        source = MapSource::alloc(fileName, std::nullopt);
        pStorage = maploadhelper::detectAndCreateStorage(source, this);
    } catch (const std::exception &ex) {
        deref(m_engine->rootContext())
            .setContextProperty("statusText",
                                tr("Cannot open file %1:\n%2.").arg(fileName, ex.what()));
        return;
    }
    connect(pStorage.get(), &AbstractMapStorage::sig_log, m_logModel, &LogModel::append);

    // Mirrors MainWindow::slot_merge()'s `getCanvas()->slot_clearAllSelections()`.
    m_mapCanvasCore->slot_clearAllSelections();

    m_ioInProgress = true;

    struct NODISCARD MergeState final
    {
        std::shared_ptr<AbstractMapStorage> storage;
        std::promise<std::optional<Map>> promise;
        std::future<std::optional<Map>> future = promise.get_future();
    };
    auto state = std::make_shared<MergeState>();
    state->storage = std::shared_ptr<AbstractMapStorage>(std::move(pStorage));
    // Snapshotted on the main thread (rather than reading m_mapData on the
    // background thread the way mainwindow-async.cpp's AsyncMerge does) so
    // this doesn't need MainWindow's CanvasDisabler/ExtraBlockers machinery
    // to keep the map from changing underneath the background worker -- an
    // immer-backed Map snapshot is cheap and inherently race-free.
    const Map currentMap = m_mapData->getCurrentMap();

    const auto handle = async_tasks::startAsyncTask2(
        AsyncTaskTypeEnum::IO,
        AllowCancelEnum::Allow,
        "Map Merge",
        [state, currentMap](const std::shared_ptr<ProgressCounter> &pc) {
            // background thread
            state->storage->setProgressCounter(pc);
            try {
                state->promise.set_value(maploadhelper::mergeMapData(*state->storage, currentMap));
            } catch (...) {
                state->promise.set_exception(std::current_exception());
            }
        },
        [this, state, fileName](const std::shared_ptr<ProgressCounter> &pc) {
            // main thread
            m_ioInProgress = false;
            endIoTaskProgress();
            const bool wasCanceled = deref(pc).hasRequestedCancel();
            std::optional<Map> result;
            try {
                result = state->future.get();
            } catch (const std::exception &ex) {
                deref(m_engine->rootContext())
                    .setContextProperty("statusText",
                                        tr("Cannot open file %1:\n%2.").arg(fileName, ex.what()));
                return;
            }

            if (wasCanceled || !result) {
                deref(m_engine->rootContext())
                    .setContextProperty("statusText",
                                        wasCanceled ? tr("Merge canceled")
                                                    : tr("Failed to merge file %1.").arg(fileName));
                return;
            }
            onSuccessfulMerge(*result);
        });
    beginIoTaskProgress(handle, tr("Merging map..."), /*cancelable=*/true);
}

void QmlShellWindow::onSuccessfulMerge(const Map &map)
{
    // Mirrors MainWindow::onSuccessfulMerge(), minus mapChanged() (widget-
    // only canvas repaint; see onSuccessfulLoad()'s doc comment for the same
    // tradeoff).
    auto &mapData = deref(m_mapData);
    mapData.setCurrentMap(map);
    mapData.checkSize();

    m_mapCanvasCore->slot_dataLoaded();
    m_groupController->slot_mapLoaded();
    updateMapModifiedState();
    deref(m_engine->rootContext()).setContextProperty("statusText", tr("File merged"));
}

void QmlShellWindow::saveMapFile(const QString &fileName,
                                 const SaveModeEnum mode,
                                 const SaveFormatEnum format)
{
    // Mirrors MainWindow::saveFile()'s per-format storage selection; the 4
    // file.export.* commands (see wireFileCommands()) share this with plain
    // file.save/file.save-as (mode=FULL, format=MM2).
    if (m_ioInProgress) {
        deref(m_engine->rootContext())
            .setContextProperty("statusText", tr("IO Task already in progress"));
        return;
    }

    std::shared_ptr<MapDestination> pDest;
    try {
        pDest = MapDestination::alloc(fileName, format);
    } catch (const std::exception &ex) {
        deref(m_engine->rootContext())
            .setContextProperty("statusText",
                                tr("Cannot set up save destination %1:\n%2.")
                                    .arg(fileName, ex.what()));
        return;
    }

    auto pStorage = std::invoke([this, &pDest, format]() -> std::unique_ptr<AbstractMapStorage> {
        const AbstractMapStorage::Data data{pDest};
        std::unique_ptr<AbstractMapStorage> storage;
        switch (format) {
        case SaveFormatEnum::MM2:
            storage = std::make_unique<MapStorage>(data, this);
            break;
        case SaveFormatEnum::MM2XML:
            storage = std::make_unique<XmlMapStorage>(data, this);
            break;
        case SaveFormatEnum::MMP:
            storage = std::make_unique<MmpMapStorage>(data, this);
            break;
        case SaveFormatEnum::WEB:
            storage = std::make_unique<JsonMapStorage>(data, this);
            break;
        }
        if (storage) {
            connect(storage.get(), &AbstractMapStorage::sig_log, m_logModel, &LogModel::append);
        }
        return storage;
    });

    if (!pStorage || !pStorage->canSave()) {
        deref(m_engine->rootContext())
            .setContextProperty("statusText", tr("Selected format cannot save."));
        return;
    }

    m_ioInProgress = true;

    struct NODISCARD SaveState final
    {
        std::shared_ptr<AbstractMapStorage> storage;
        std::shared_ptr<MapDestination> destination;
        std::promise<std::optional<bool>> promise;
        std::future<std::optional<bool>> future = promise.get_future();
    };
    auto state = std::make_shared<SaveState>();
    state->storage = std::shared_ptr<AbstractMapStorage>(std::move(pStorage));
    state->destination = pDest;

    const auto handle = async_tasks::startAsyncTask2(
        AsyncTaskTypeEnum::IO,
        AllowCancelEnum::Forbid,
        "Map Saver",
        [this, state, mode](const std::shared_ptr<ProgressCounter> &pc) {
            // background thread. `this` is safe to dereference here: the
            // async task engine is drained by async_tasks::cleanup() in
            // ~QmlShellWindow() before any member it touches is destroyed
            // (see this class's dtor comment).
            state->storage->setProgressCounter(pc);
            try {
                state->promise.set_value(
                    state->storage->saveData(deref(m_mapData), mode == SaveModeEnum::BASEMAP));
            } catch (...) {
                state->promise.set_exception(std::current_exception());
            }
        },
        [this, state, fileName, mode, format](const std::shared_ptr<ProgressCounter> &) {
            // main thread
            m_ioInProgress = false;
            endIoTaskProgress();
            state->destination->finalize();

            bool success = false;
            try {
                const std::optional<bool> result = state->future.get();
                success = result.has_value() && result.value();
            } catch (const std::exception &ex) {
                deref(m_engine->rootContext())
                    .setContextProperty("statusText",
                                        tr("Cannot save file %1:\n%2.").arg(fileName, ex.what()));
                return;
            }

            if (!success) {
                deref(m_engine->rootContext())
                    .setContextProperty("statusText", tr("Failed to save file %1.").arg(fileName));
                return;
            }

            // Mirrors MainWindow::onSuccessfulSave(): only a plain full MM2
            // save rebinds MapData's own current-file tracking (see
            // loadFile()'s doc comment); an export leaves the currently
            // open map/filename untouched. Unlike MainWindow, this skips
            // the "Autoload this map?" follow-up QMessageBox (documented
            // deviation, see the task report) but still updates
            // lastMapDirectory for every successful save/export, matching
            // MainWindow::onSuccessfulSave()'s unconditional config update.
            if (mode == SaveModeEnum::FULL && format == SaveFormatEnum::MM2) {
                m_mapData->setFileName(fileName, !QFileInfo(fileName).isWritable());
                m_mapData->currentHasBeenSaved();
                updateMapModifiedState();
            }
            setConfig().autoLoad.lastMapDirectory = QFileInfo(fileName).dir().absolutePath();
            deref(m_engine->rootContext()).setContextProperty("statusText", tr("File saved"));
        });
    beginIoTaskProgress(handle, tr("Saving map..."), /*cancelable=*/false);
}

void QmlShellWindow::beginIoTaskProgress(async_tasks::AsyncTaskHandle handle,
                                         const QString &label,
                                         const bool cancelable)
{
    m_ioTaskHandle = std::move(handle);
    if (m_ioTaskController != nullptr) {
        m_ioTaskController->begin(label, cancelable);
    }
    if (m_ioProgressTimer != nullptr) {
        m_ioProgressTimer->start();
    }
}

void QmlShellWindow::tickIoTaskProgress()
{
    // Mirrors MainWindow::AsyncIO::updateStatus()'s 25ms QTimer poll of the
    // running task's ProgressCounter -- see m_ioProgressTimer's setup in
    // this class's ctor -- except this always re-pushes the current status
    // to IoTaskController (which itself only emits sig_changed(), so QML
    // bindings only actually re-evaluate on a real change) rather than
    // diff-checking against a remembered last message/percent the way
    // AsyncIO::updateStatus() does; simpler, and cheap enough at 25ms for a
    // single popup.
    if (!m_ioTaskHandle) {
        if (m_ioProgressTimer != nullptr) {
            m_ioProgressTimer->stop();
        }
        return;
    }
    const ProgressCounter::Status status = m_ioTaskHandle->getStatus();
    if (m_ioTaskController != nullptr) {
        // Clamped to 99, same as AsyncIO::updateStatus(): 100% is reserved
        // for "the task has actually finished", which this popup instead
        // represents by disappearing (see endIoTaskProgress()).
        const int percent = static_cast<int>(std::min<std::size_t>(status.percent(), 99));
        m_ioTaskController->update(status.msg.toQString(), percent);
    }
}

void QmlShellWindow::endIoTaskProgress()
{
    if (m_ioProgressTimer != nullptr) {
        m_ioProgressTimer->stop();
    }
    m_ioTaskHandle.reset();
    if (m_ioTaskController != nullptr) {
        m_ioTaskController->end();
    }
}

void QmlShellWindow::updateMapModifiedState()
{
    const bool modified = m_mapData->dataChanged();
    if (UiCommand *const cmd = m_commandRegistry->command("file.save")) {
        cmd->setEnabled(modified);
    }
    if (modified) {
        hideSplash();
    }
    updateWindowTitle();
}

void QmlShellWindow::updateWindowTitle()
{
    // Mirrors MainWindow::setCurrentFile(), minus the "[*]" unsaved-changes
    // placeholder QWidget::windowTitle() understands natively -- QQC2.
    // ApplicationWindow's title has no equivalent built-in modified-marker
    // syntax, so this appends a literal "*" instead when the map has
    // unsaved changes.
    if (m_engine == nullptr) {
        return;
    }
    const QString &fileName = m_mapData->getFileName();
    const QFileInfo file(fileName);
    const QString shownName = fileName.isEmpty() ? QStringLiteral("Untitled") : file.fileName();
    const QString fileSuffix = (m_mapData->isFileReadOnly()
                                || (!fileName.isEmpty() && !file.isWritable()))
                                   ? QStringLiteral(" [read-only]")
                                   : QString{};
    const QString modifiedMark = m_mapData->dataChanged() ? QStringLiteral("*") : QString{};
    const QString appSuffix = isMMapperBeta() ? QStringLiteral(" Beta") : QString{};
    const QString title = QStringLiteral("%1%2%3 - MMapper%4")
                              .arg(shownName, modifiedMark, fileSuffix, appSuffix);
    m_engine->rootContext()->setContextProperty("windowTitle", title);
}

void QmlShellWindow::hideSplash()
{
    // Mirrors the two call sites of MapWindow::hideSplashImage() (widget
    // shell): mainwindow/utils.cpp's CanvasDisabler destructor, which fires
    // unconditionally once any load/merge async task finishes (success or
    // failure) -- reproduced by loadFile()'s async completion callback --
    // and MainWindow::setMapModified(true), fired whenever the map is
    // edited -- reproduced by updateMapModifiedState(). There is no
    // un-hide: once the splash is hidden it stays hidden, matching
    // hideSplashImage()'s own one-way QPointer/deleteLater() widget
    // teardown.
    if (m_splashHidden || m_engine == nullptr) {
        return;
    }
    m_splashHidden = true;
    m_engine->rootContext()->setContextProperty("mapLoaded", true);
}
