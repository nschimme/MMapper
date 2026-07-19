// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "QmlShellWindow.h"

#include "../adventure/AdventureLogModel.h"
#include "../adventure/adventuretracker.h"
#include "../client/ClientController.h"
#include "../client/ClientLineModel.h"
#include "../client/HotkeyManager.h"
#include "../display/CanvasMouseModeEnum.h"
#include "../display/MapCanvasCore.h"
#include "../display/MapViewModel.h"
#include "../display/prespammedpath.h"
#include "../global/utils.h"
#include "../group/GroupController.h"
#include "../group/GroupModel.h"
#include "../group/mmapper2group.h"
#include "../mapdata/mapdata.h"
#include "../media/DescriptionAdapter.h"
#include "../media/MediaLibrary.h"
#include "../observer/gameobserver.h"
#include "../proxy/GmcpMessage.h"
#include "../qml/DescriptionImageProvider.h"
#include "../qml/DockLayoutController.h"
#include "../qml/GroupIconProvider.h"
#include "../qml/QmlConfig.h"
#include "../roompanel/RoomManager.h"
#include "../roompanel/RoomModel.h"
#include "../timers/CTimers.h"
#include "../timers/TimerController.h"
#include "../timers/TimerModel.h"
#include "CommandRegistry.h"
#include "LogModel.h"
#include "TasksModel.h"
#include "UiCommand.h"

#include <array>

#include <QApplication>
#include <QCoreApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QUrl>

namespace { // anonymous

struct NODISCARD CommandSpec final
{
    const char *id = nullptr;
    bool checkable = false;
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
// listener, ...).
constexpr std::array<CommandSpec, 65> ALL_COMMAND_SPECS = {
    CommandSpec{"file.new", false},
    CommandSpec{"file.open", false},
    CommandSpec{"file.merge", false},
    CommandSpec{"file.reload", false},
    CommandSpec{"file.save", false},
    CommandSpec{"file.save-as", false},
    CommandSpec{"file.export.base-map", false},
    CommandSpec{"file.export.mm2xml", false},
    CommandSpec{"file.export.web", false},
    CommandSpec{"file.export.mmp", false},
    CommandSpec{"file.exit", false},
    CommandSpec{"edit.undo", false},
    CommandSpec{"edit.redo", false},
    CommandSpec{"edit.preferences", false},
    CommandSpec{"view.zoom-in", false},
    CommandSpec{"view.zoom-out", false},
    CommandSpec{"view.zoom-reset", false},
    CommandSpec{"view.always-on-top", true},
    CommandSpec{"view.show-status-bar", true},
    CommandSpec{"view.show-scroll-bars", true},
    CommandSpec{"view.show-menu-bar", true},
    CommandSpec{"layer.up", false},
    CommandSpec{"layer.down", false},
    CommandSpec{"layer.reset", false},
    CommandSpec{"mouse-mode.move", true},
    CommandSpec{"mouse-mode.room-raypick", true},
    CommandSpec{"mouse-mode.room-select", true},
    CommandSpec{"mouse-mode.connection-select", true},
    CommandSpec{"mouse-mode.create-room", true},
    CommandSpec{"mouse-mode.create-connection", true},
    CommandSpec{"mouse-mode.create-oneway-connection", true},
    CommandSpec{"mouse-mode.infomark-select", true},
    CommandSpec{"mouse-mode.create-infomark", true},
    CommandSpec{"mapper-mode.play", true},
    CommandSpec{"mapper-mode.map", true},
    CommandSpec{"mapper-mode.offline", true},
    CommandSpec{"room.create", false},
    CommandSpec{"room.edit-selected", false},
    CommandSpec{"room.delete-selected", false},
    CommandSpec{"room.move-up-selected", false},
    CommandSpec{"room.move-down-selected", false},
    CommandSpec{"room.merge-up-selected", false},
    CommandSpec{"room.merge-down-selected", false},
    CommandSpec{"room.connect-to-neighbours", false},
    CommandSpec{"room.find", false},
    CommandSpec{"room.goto-selected", false},
    CommandSpec{"room.force-update-selected", false},
    CommandSpec{"connection.delete-selected", false},
    CommandSpec{"infomark.edit-selected", true},
    CommandSpec{"infomark.delete-selected", true},
    CommandSpec{"client.launch", false},
    CommandSpec{"client.save-log", false},
    CommandSpec{"client.save-log-html", false},
    CommandSpec{"pathmachine.release-all-paths", false},
    CommandSpec{"world.rebuild-meshes", false},
    CommandSpec{"help.vote", false},
    CommandSpec{"help.check-for-update", false},
    CommandSpec{"help.website", false},
    CommandSpec{"help.forum", false},
    CommandSpec{"help.wiki", false},
    CommandSpec{"help.setup", false},
    CommandSpec{"help.newbie", false},
    CommandSpec{"help.report-issue", false},
    CommandSpec{"help.about", false},
    CommandSpec{"help.about-qt", false},
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
        "file.exit",
        "view.zoom-in",
        "view.zoom-out",
        "view.zoom-reset",
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
    };
    return live.contains(id);
}

} // namespace

QmlShellWindow::QmlShellWindow(QObject *const parent)
    : QObject(parent)
{
    // --- minimal offline service set MapCanvasCore's constructor needs
    // (see display/MapCanvasCore.h) -- mirrors the construction order in
    // MainWindow::MainWindow() (mainwindow.cpp), minus everything that
    // needs the async task engine, the proxy/listener, or QtWidgets. ---
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

    m_engine = new QQmlApplicationEngine(this);
    m_engine->rootContext()->setContextProperty("commands", m_commandRegistry);
    m_engine->rootContext()->setContextProperty("mapCore", m_mapCanvasCore);
    m_engine->rootContext()->setContextProperty("mapViewModel", m_mapViewModel);
    // TODO(shell commit): drive this from AppCore::sig_statusMessage once
    // AppCore's canvas-facing methods are ported from MapCanvas (widget) to
    // MapCanvasCore -- see AppCore.h's m_canvas, which is still typed
    // MapCanvas*. Until then this is a static placeholder string.
    m_engine->rootContext()->setContextProperty("statusText",
                                                QStringLiteral(
                                                    "MMapper QML shell preview (offline)"));

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

    m_engine->load(QUrl(QStringLiteral("qrc:/qt/qml/MMapper/MainShell.qml")));
    m_valid = !m_engine->rootObjects().isEmpty();
}

QmlShellWindow::~QmlShellWindow() = default;

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
