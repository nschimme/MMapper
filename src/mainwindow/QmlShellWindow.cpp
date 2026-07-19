// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "QmlShellWindow.h"

#include "../adventure/AdventureLogModel.h"
#include "../adventure/XpStatusAdapter.h"
#include "../adventure/adventuretracker.h"
#include "../client/ClientController.h"
#include "../client/ClientLineModel.h"
#include "../client/HotkeyManager.h"
#include "../clock/ClockAdapter.h"
#include "../clock/mumeclock.h"
#include "../configuration/configuration.h"
#include "../display/CanvasMouseModeEnum.h"
#include "../display/InfomarkSelection.h"
#include "../display/MapCanvasCore.h"
#include "../display/MapViewModel.h"
#include "../display/MapZoomController.h"
#include "../display/prespammedpath.h"
#include "../global/AsyncTasks.h"
#include "../global/ConfigConsts.h"
#include "../global/utils.h"
#include "../group/GroupController.h"
#include "../group/GroupModel.h"
#include "../group/mmapper2group.h"
#include "../mapdata/mapdata.h"
#include "../mapdata/roomselection.h"
#include "../mapstorage/MapDestination.h"
#include "../mapstorage/MapLoadHelper.h"
#include "../mapstorage/MapSource.h"
#include "../mapstorage/abstractmapstorage.h"
#include "../mapstorage/mapstorage.h"
#include "../media/DescriptionAdapter.h"
#include "../media/MediaLibrary.h"
#include "../observer/gameobserver.h"
#include "../preferences/GeneralPageAdapter.h"
#include "../preferences/ParserPageAdapter.h"
#include "../preferences/PreferencesController.h"
#include "../proxy/GmcpMessage.h"
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
#include "AudioVolumeController.h"
#include "CommandRegistry.h"
#include "FindRoomsController.h"
#include "FindRoomsModel.h"
#include "InfomarkEditController.h"
#include "LogModel.h"
#include "RoomEditController.h"
#include "TasksModel.h"
#include "UiCommand.h"
#include "UpdateChecker.h"
#include "metatypes.h"

#include <array>
#include <future>
#include <memory>
#include <optional>
#include <tuple>

#include <QApplication>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDataStream>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QMessageBox>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QRect>
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
        "file.open",
        "file.save",
        "file.save-as",
        "file.exit",
        "view.zoom-in",
        "view.zoom-out",
        "view.zoom-reset",
        "view.always-on-top",
        "view.show-status-bar",
        "view.show-scroll-bars",
        "layer.up",
        "layer.down",
        "layer.reset",
        "world.rebuild-meshes",
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
        "help.check-for-update",
        // room.edit-selected/infomark.edit-selected are registered live but
        // START disabled (see registerCommands()'s
        // "no selection yet" comment below) -- wireSelectionCommands()
        // toggles them as the canvas selection changes, mirroring
        // MainWindow::slot_newRoomSelection()/slot_newInfomarkSelection()'s
        // m_commandRegistry->command(...)->setEnabled() calls (see
        // AppCore::onNewRoomSelection()).
        "room.edit-selected",
        "infomark.edit-selected",
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

    m_timers = new CTimers(this);
    m_timerModel = new TimerModel(deref(m_timers), this);
    m_timerController = new TimerController(deref(m_timers), deref(m_timerModel), this);

    m_tasksModel = new TasksModel(this);

    m_hotkeyManager = std::make_unique<HotkeyManager>();
    m_clientLineModel = new ClientLineModel(this);
    m_clientController = new ClientController(deref(m_clientLineModel),
                                              deref(m_hotkeyManager),
                                              this);
    // No ClientControllerBackend installed -- this shell has no
    // ConnectionListener/telnet proxy to back it with (see file comment).
    // Every backend-driving invokable on m_clientController silently no-ops
    // until a later commit wires one up.

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
    // TODO(shell commit): drive this from AppCore::sig_statusMessage once
    // AppCore's canvas-facing methods are ported from MapCanvas (widget) to
    // MapCanvasCore -- see AppCore.h's m_canvas, which is still typed
    // MapCanvas*. Until then this is a static placeholder string, updated
    // only by the XpStatusAdapter show/clear-status-message signals below
    // (see MainShell.qml's footer statusText handling).
    m_engine->rootContext()->setContextProperty("statusText",
                                                QStringLiteral(
                                                    "MMapper QML shell preview (offline)"));
    // Splash-overlay visibility (MapView.qml) and window title (MainShell.qml's
    // ApplicationWindow.title) -- see hideSplash()/updateWindowTitle(), called
    // from the file I/O paths wired below in wireFileCommands().
    m_engine->rootContext()->setContextProperty("mapLoaded", false);
    m_engine->rootContext()->setContextProperty("windowTitle",
                                                QStringLiteral("MMapper (QML shell preview)"));

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
    // / XpStatusAdapter show/clear-status-message wiring; see this ctor's
    // "statusText" comment above for what's still missing (full AppCore
    // integration). setContextProperty() re-fires bindings that read
    // "statusText" the same way a Q_PROPERTY NOTIFY would.
    static const QString defaultStatus = QStringLiteral("MMapper QML shell preview (offline)");
    connect(m_xpStatusAdapter,
            &XpStatusAdapter::sig_showStatusMessage,
            this,
            [rootContext](const QString &msg) {
                rootContext->setContextProperty("statusText", msg);
            });
    connect(m_xpStatusAdapter, &XpStatusAdapter::sig_clearStatusMessage, this, [rootContext]() {
        rootContext->setContextProperty("statusText", defaultStatus);
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
}

QmlShellWindow::~QmlShellWindow()
{
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
    if (!geometry.isEmpty()) {
        QDataStream in(geometry);
        QRect rect;
        in >> rect;
        if (in.status() == QDataStream::Ok && rect.isValid()) {
            window->setGeometry(rect);
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

    connect(m_commandRegistry->command("file.exit"),
            &UiCommand::sig_triggered,
            qApp,
            &QCoreApplication::quit);

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
    // room.edit-selected/infomark.edit-selected are registered live (see
    // isLiveCommand()) but start with no selection to act on.
    if (UiCommand *const cmd = m_commandRegistry->command("room.edit-selected")) {
        cmd->setEnabled(false);
    }
    if (UiCommand *const cmd = m_commandRegistry->command("infomark.edit-selected")) {
        cmd->setEnabled(false);
    }

    // Mirrors MainWindow::slot_newRoomSelection()/slot_newInfomarkSelection():
    // track the canvas's current selection so the edit commands can be
    // enabled/disabled and the edit dialogs constructed with the right
    // selection.
    connect(m_mapCanvasCore,
            &MapCanvasCore::sig_newRoomSelection,
            this,
            [this](const SigRoomSelection &rs) {
                m_roomSelection = !rs.isValid() ? nullptr : rs.getShared();
                if (UiCommand *const cmd = m_commandRegistry->command("room.edit-selected")) {
                    cmd->setEnabled(m_roomSelection != nullptr);
                }
            });
    connect(m_mapCanvasCore,
            &MapCanvasCore::sig_newInfomarkSelection,
            this,
            [this](InfomarkSelection *const is) {
                const bool isNonNull = is != nullptr;
                m_infoMarkSelection = isNonNull ? is->shared_from_this() : nullptr;
                if (UiCommand *const cmd = m_commandRegistry->command("infomark.edit-selected")) {
                    cmd->setEnabled(m_infoMarkSelection != nullptr);
                }
            });

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
}

void QmlShellWindow::wireFileCommands()
{
    // file.save starts disabled: mirrors MainWindow's saveAct, which stays
    // disabled until MainWindow::setMapModified(true) enables it (see
    // mainwindow.cpp's wireConnections()/onSuccessfulLoad()) -- there is
    // nothing to save until a map is loaded or edited. file.open/
    // file.save-as have no such precondition, and were left enabled by
    // registerCommands() (see isLiveCommand()).
    if (UiCommand *const cmd = m_commandRegistry->command("file.save")) {
        cmd->setEnabled(false);
    }

    // Mirrors MainWindow::wireConnections()'s
    // `connect(m_mapData, &MapData::sig_onDataChanged, ...)`: the "map was
    // edited" splash-hide/save-enable/title-refresh trigger (see
    // updateMapModifiedState()'s doc comment).
    connect(m_mapData, &MapData::sig_onDataChanged, this, &QmlShellWindow::updateMapModifiedState);

    connect(m_commandRegistry->command("file.open"), &UiCommand::sig_triggered, this, [this]() {
        // Mirrors MainWindow::slot_open()'s non-Wasm branch: same name
        // filter and lastMapDirectory config key as the widget shell (see
        // mainwindow.cpp), so both shells remember the same "last opened"
        // directory. This shell is desktop-only (never constructed under
        // Q_OS_WASM -- see QmlShellWindow.h's file comment), so there is no
        // QFileDialog::getOpenFileContent() branch to mirror.
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

    connect(m_commandRegistry->command("file.save"), &UiCommand::sig_triggered, this, [this]() {
        // Mirrors MainWindow::slot_save().
        const QString &fileName = m_mapData->getFileName();
        if (fileName.isEmpty() || m_mapData->isFileReadOnly()) {
            if (UiCommand *const cmd = m_commandRegistry->command("file.save-as")) {
                cmd->trigger();
            }
            return;
        }
        saveMapFile(fileName);
    });

    connect(m_commandRegistry->command("file.save-as"), &UiCommand::sig_triggered, this, [this]() {
        // Mirrors MainWindow::slot_saveAs(), using a plain
        // QFileDialog::getSaveFileName() instead of mainwindow-saveslots.cpp's
        // createDefaultSaveDialog() helper (same filter/suggested-name logic,
        // just without that file's reusable multi-format dialog builders --
        // this shell only ever saves MM2; see saveMapFile()'s doc comment).
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
            return;
        }
        saveMapFile(fileName);
    });
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
    // MainWindow::loadFile()'s forceNewFile() call. This shell has no
    // Mmapper2PathMachine or AudioManager yet (see QmlShellWindow.h's file
    // comment), so there is nothing to reset for them.
    {
        auto &mapData = deref(m_mapData);
        mapData.clear();
        mapData.setFileName(QString{}, false);
        mapData.forcePosition(Coordinate{});
    }
    m_mapCanvasCore->slot_dataLoaded();
    m_groupController->slot_mapLoaded();
    m_descriptionAdapter->updateRoom(RoomHandle{});
    updateWindowTitle();

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

    // No QProgressDialog here (unlike MainWindow::loadFile()'s
    // beginAsyncIO()) -- this shell shows load/save progress via the Tasks
    // panel/tasksModel instead, which polls the same async_tasks registry
    // this call starts a task in (see QmlShellWindow.h's loadFile() doc
    // comment and TasksModel.h).
    std::ignore = async_tasks::startAsyncTask2(
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
}

void QmlShellWindow::onSuccessfulLoad(const MapLoadData &mapLoadData)
{
    // Mirrors MainWindow::onSuccessfulLoad() (mainwindow.cpp), minus:
    //  - Mmapper2PathMachine::onMapLoaded() -- this shell has no path
    //    machine yet (see QmlShellWindow.h's file comment);
    //  - AudioManager::onAreaChanged() -- this shell has no AudioManager;
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
    if (const auto room = mapData.getCurrentRoom()) {
        m_descriptionAdapter->updateRoom(room);
    }

    updateMapModifiedState();
    deref(m_engine->rootContext()).setContextProperty("statusText", tr("File loaded"));
}

void QmlShellWindow::saveMapFile(const QString &fileName)
{
    // Mirrors MainWindow::saveFile(), minus the QProgressDialog (see
    // loadFile()'s doc comment) and minus the basemap/XML/web/MMP export
    // formats (file.export.* stay disabled placeholders -- see
    // ALL_COMMAND_SPECS/isLiveCommand(); MainWindow's export slots pull in
    // mainwindow-saveslots.cpp's multi-format save-dialog helpers, which
    // this preview shell's file.save/file.save-as don't need). Only the
    // plain MM2 format is wired.
    if (m_ioInProgress) {
        deref(m_engine->rootContext())
            .setContextProperty("statusText", tr("IO Task already in progress"));
        return;
    }

    std::shared_ptr<MapDestination> pDest;
    try {
        pDest = MapDestination::alloc(fileName, SaveFormatEnum::MM2);
    } catch (const std::exception &ex) {
        deref(m_engine->rootContext())
            .setContextProperty("statusText",
                                tr("Cannot set up save destination %1:\n%2.")
                                    .arg(fileName, ex.what()));
        return;
    }

    auto pStorage = std::invoke([this, &pDest]() -> std::unique_ptr<AbstractMapStorage> {
        const AbstractMapStorage::Data data{pDest};
        auto storage = std::make_unique<MapStorage>(data, this);
        connect(storage.get(), &AbstractMapStorage::sig_log, m_logModel, &LogModel::append);
        return storage;
    });

    if (!pStorage->canSave()) {
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

    std::ignore = async_tasks::startAsyncTask2(
        AsyncTaskTypeEnum::IO,
        AllowCancelEnum::Forbid,
        "Map Saver",
        [this, state](const std::shared_ptr<ProgressCounter> &pc) {
            // background thread. `this` is safe to dereference here: the
            // async task engine is drained by async_tasks::cleanup() in
            // ~QmlShellWindow() before any member it touches is destroyed
            // (see this class's dtor comment).
            state->storage->setProgressCounter(pc);
            try {
                state->promise.set_value(state->storage->saveData(deref(m_mapData), false));
            } catch (...) {
                state->promise.set_exception(std::current_exception());
            }
        },
        [this, state, fileName](const std::shared_ptr<ProgressCounter> &) {
            // main thread
            m_ioInProgress = false;
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

            m_mapData->setFileName(fileName, !QFileInfo(fileName).isWritable());
            m_mapData->currentHasBeenSaved();
            setConfig().autoLoad.lastMapDirectory = QFileInfo(fileName).dir().absolutePath();
            updateMapModifiedState();
            deref(m_engine->rootContext()).setContextProperty("statusText", tr("File saved"));
        });
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
    const QString title = QStringLiteral("%1%2%3 - MMapper (QML shell preview)")
                              .arg(shownName, modifiedMark, fileSuffix);
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
