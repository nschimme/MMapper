// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "mainwindow.h"

#include "../adventure/AdventureLogModel.h"
#include "../adventure/adventuretracker.h"
#ifndef MMAPPER_WITH_QML
#include "../adventure/adventurewidget.h"
#include "../adventure/xpstatuswidget.h"
#endif
#ifndef MMAPPER_WITH_QML
#include "../client/ClientWidget.h"
#endif
#include "../client/HotkeyManager.h"
#include "../clock/mumeclock.h"
#ifndef MMAPPER_WITH_QML
#include "../clock/mumeclockwidget.h"
#endif
#include "../display/InfomarkSelection.h"
#include "../display/MapCanvasData.h"
#include "../display/mapcanvas.h"
#include "../display/mapwindow.h"
#include "../global/AsyncTasks.h"
#include "../global/PrintUtils.h"
#include "../global/SendToUser.h"
#include "../global/Version.h"
#include "../global/window_utils.h"
#ifndef MMAPPER_WITH_QML
#include "../group/groupwidget.h"
#endif
#include "../logger/autologger.h"
#include "../media/AudioManager.h"
#ifndef MMAPPER_WITH_QML
#include "../media/DescriptionWidget.h"
#endif
#include "../media/MediaLibrary.h"
#include "../pathmachine/mmapper2pathmachine.h"
#ifndef MMAPPER_WITH_QML
#include "../preferences/configdialog.h"
#endif
#include "../proxy/connectionlistener.h"
#include "../roompanel/RoomManager.h"
#ifndef MMAPPER_WITH_QML
#include "../roompanel/RoomWidget.h"
#endif
#include "../timers/CTimers.h"
#ifndef MMAPPER_WITH_QML
#include "../timers/TimerWidget.h"
#endif
#include "../viewers/TopLevelWindows.h"
#include "AudioVolumeSlider.h"
#include "MapZoomSlider.h"
#ifndef MMAPPER_WITH_QML
#include "TasksPanel.h"
#endif
#ifndef MMAPPER_WITH_QML
#include "UpdateDialog.h"
#include "aboutdialog.h"
#endif
#ifndef MMAPPER_WITH_QML
#include "findroomsdlg.h"
#endif
#ifndef MMAPPER_WITH_QML
#include "infomarkseditdlg.h"
#endif
#ifndef MMAPPER_WITH_QML
#include "roomeditattrdlg.h"
#endif
#include "mainwindow-async.h"
#include "metatypes.h"
#include "utils.h"

#ifdef MMAPPER_WITH_QML
#include "../adventure/XpStatusAdapter.h"
#include "../client/ClientController.h"
#include "../client/ClientLineModel.h"
#include "../client/ClientTelnetBackend.h"
#include "../clock/ClockAdapter.h"
#include "../group/GroupController.h"
#include "../media/DescriptionAdapter.h"
#include "../preferences/ParserPageAdapter.h"
#include "../preferences/PreferencesController.h"
#include "../qml/AnsiColorPickerLauncher.h"
#include "../qml/DescriptionImageProvider.h"
#include "../qml/GroupIconProvider.h"
#include "../qml/QmlConfig.h"
#include "../qml/QmlDialog.h"
#include "../qml/QmlDockWidget.h"
#include "../roompanel/RoomModel.h"
#include "../timers/TimerController.h"
#include "../timers/TimerModel.h"
#include "AboutInfo.h"
#include "FindRoomsController.h"
#include "FindRoomsModel.h"
#include "InfomarkEditController.h"
#include "LogModel.h"
#include "RoomEditController.h"
#include "TasksModel.h"
#include "UpdateChecker.h"

#include <QQmlContext>
#include <QQuickWidget>
#endif

#include <memory>
#include <mutex>

#include <QActionGroup>
#include <QCloseEvent>
#include <QFileDialog>
#include <QFontDatabase>
#include <QIcon>
#include <QProgressDialog>
#include <QSize>
#include <QString>
#include <QTextBrowser>
#include <QUrl>
#include <QtWidgets>

NODISCARD static const char *get_type_name(const AsyncIOTypeEnum mode)
{
    switch (mode) {
        using enum AsyncIOTypeEnum;
    case Load:
        return "load";
    case Merge:
        return "merge";
    case Save:
        return "save";
    default:
        assert(false);
        return "(error)";
    }
}

NODISCARD static const char *basic_plural(size_t n)
{
    return (n == 1) ? "" : "s";
}

static void addApplicationFont()
{
    const auto id = QFontDatabase::addApplicationFont(":/fonts/DejaVuSansMono.ttf");
    const auto family = QFontDatabase::applicationFontFamilies(id);
    if (family.isEmpty()) {
        qWarning() << "Unable to load bundled DejaVuSansMono font";
    } else {
        // Use the application font here because we can guarantee that resources have been loaded.
        // REVISIT: Move this to the configuration?
        if (getConfig().integratedClient.font.isEmpty()) {
            // TODO: Explain why mac is 20% larger.
            static constinit int defaultFontSize = (CURRENT_PLATFORM == PlatformEnum::Mac) ? 12
                                                                                           : 10;
            QFont defaultClientFont;
            defaultClientFont.setFamily(family.front());
            defaultClientFont.setPointSize(defaultFontSize);
            defaultClientFont.setStyleStrategy(QFont::PreferAntialias);
            setConfig().integratedClient.font = defaultClientFont.toString();
        }
    }
}

MainWindow::~MainWindow()
{
    g_mainWindow = nullptr;
    mmqt::rdisconnect(this);
    async_tasks::cleanup();
    delete m_listener;
    destroyTopLevelWindows();
}

MainWindow::MainWindow()
    : QMainWindow(nullptr, Qt::WindowFlags{})
    , m_asyncIO{std::make_unique<AsyncIO>(this)}
{
    initTopLevelWindows();
    async_tasks::init();
    setObjectName("MainWindow");
    setWindowIcon(QIcon(":/icons/mmapper-lo.svg"));
    addApplicationFont();
    registerMetatypes();

#ifdef MMAPPER_WITH_QML
    m_qmlConfig = new QmlConfig(this);
#endif

    std::invoke([this] {
        auto *const mapData = new MapData(this);
        mapData->setObjectName("MapData");

        m_mapData = mapData;
    });

    setCurrentFile("");

    m_prespammedPath = new PrespammedPath(this);

    m_groupManager = new Mmapper2Group(this);
    m_groupManager->setObjectName("GroupManager");

    m_gameObserver = std::make_unique<GameObserver>();

    m_mapWindow = new MapWindow(deref(m_mapData),
                                deref(m_gameObserver),
                                deref(m_prespammedPath),
                                deref(m_groupManager),
                                this);
    setCentralWidget(m_mapWindow);

    std::invoke([this] {
        auto *const pathMachine = new Mmapper2PathMachine(deref(m_mapData), this);
        pathMachine->setObjectName("Mmapper2PathMachine");

        m_pathMachine = pathMachine;
    });

    m_mediaLibrary = new MediaLibrary(this);
    m_audioManager = new AudioManager(deref(m_mediaLibrary), deref(m_gameObserver), this);

    // View -> Side Panels -> Log Panel
    std::invoke([this] {
#ifdef MMAPPER_WITH_QML
        auto *const model = new LogModel(this);
        auto *const dock = new QmlDockWidget(tr("Log Panel"), "DockWidgetLog", this);
        dock->setAllowedAreas(Qt::TopDockWidgetArea | Qt::BottomDockWidgetArea);
        dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable
                          | QDockWidget::DockWidgetClosable);
        dock->toggleViewAction()->setShortcut(tr("Ctrl+L"));
        addDockWidget(Qt::BottomDockWidgetArea, dock);
        dock->setContextProperty("logModel", model);
        dock->setQmlSource(QUrl(QStringLiteral("qrc:/qt/qml/MMapper/LogPanel.qml")));
        dock->hide();

        m_logModel = model;
#else
        auto *const dock = new QDockWidget(tr("Log Panel"), this);
        dock->setObjectName("DockWidgetLog");
        dock->setAllowedAreas(Qt::TopDockWidgetArea | Qt::BottomDockWidgetArea);
        dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable
                          | QDockWidget::DockWidgetClosable);
        dock->toggleViewAction()->setShortcut(tr("Ctrl+L"));
        addDockWidget(Qt::BottomDockWidgetArea, dock);

        auto *const logWindow = new QTextBrowser(m_dockDialogLog);
        logWindow->setReadOnly(true);
        logWindow->setObjectName("LogWindow");
        dock->setWidget(logWindow);
        dock->hide();

        m_logWindow = logWindow;
#endif

        m_dockDialogLog = dock;
    });

    // View -> Side Panels -> Group Panel and Tools -> Group Manager
    std::invoke([this] {
#ifdef MMAPPER_WITH_QML
        auto *const controller = new GroupController(deref(m_groupManager), deref(m_mapData), this);
        auto *const dock = new QmlDockWidget(tr("Group Panel"), "DockWidgetGroup", this);
        dock->setAllowedAreas(Qt::TopDockWidgetArea | Qt::BottomDockWidgetArea);
        dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable
                          | QDockWidget::DockWidgetClosable);
        addDockWidget(Qt::TopDockWidgetArea, dock);
        dock->addImageProvider("groupicons", new GroupIconProvider());
        dock->setContextProperty("groupModel", controller->getModel());
        dock->setContextProperty("groupProxyModel", controller->getProxy());
        dock->setContextProperty("groupController", controller);
        dock->setContextProperty("config", m_qmlConfig);
        dock->setQmlSource(QUrl(QStringLiteral("qrc:/qt/qml/MMapper/GroupPanel.qml")));
        connect(controller,
                &GroupController::sig_center,
                m_mapWindow,
                &MapWindow::slot_centerOnWorldPos);
        connect(m_qmlConfig,
                &QmlConfig::npcHideChanged,
                controller,
                &GroupController::refreshFilter);
        connect(m_qmlConfig,
                &QmlConfig::npcSortBottomChanged,
                controller,
                &GroupController::refreshFilter);

        m_groupController = controller;
#else
        auto *const w = new GroupWidget(m_groupManager, m_mapData, this);
        auto *const dock = new QDockWidget(tr("Group Panel"), this);
        dock->setObjectName("DockWidgetGroup");
        dock->setAllowedAreas(Qt::TopDockWidgetArea | Qt::BottomDockWidgetArea);
        dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable
                          | QDockWidget::DockWidgetClosable);
        addDockWidget(Qt::TopDockWidgetArea, dock);
        dock->setWidget(w);
        connect(w, &GroupWidget::sig_center, m_mapWindow, &MapWindow::slot_centerOnWorldPos);

        m_groupWidget = w;
#endif
        m_dockDialogGroup = dock;
    });

    // View -> Side Panels -> Room Panel (Mobs)
    std::invoke([this] {
        auto *const roomManager = new RoomManager(this);
        roomManager->setObjectName("RoomManager");

        deref(m_gameObserver)
            .sig2_sentToUserGmcp.connect(m_lifetime, [this](const GmcpMessage &gmcp) {
                deref(m_roomManager).slot_parseGmcpInput(gmcp);
            });

        m_roomManager = roomManager;
    });

    std::invoke([this] {
#ifdef MMAPPER_WITH_QML
        auto *const model = new RoomModel(this, deref(m_roomManager).getRoom());
        connect(m_roomManager, &RoomManager::sig_updateWidget, model, &RoomModel::update);

        auto *const dock = new QmlDockWidget(tr("Room Panel"), "DockWidgetRoom", this);
        dock->setAllowedAreas(Qt::TopDockWidgetArea | Qt::BottomDockWidgetArea);
        dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable
                          | QDockWidget::DockWidgetClosable);
        addDockWidget(Qt::BottomDockWidgetArea, dock);
        dock->setContextProperty("roomModel", model);
        dock->setQmlSource(QUrl(QStringLiteral("qrc:/qt/qml/MMapper/RoomPanel.qml")));
        dock->hide();
#else
        auto *const w = new RoomWidget(deref(m_roomManager), this);
        auto *const dock = new QDockWidget(tr("Room Panel"), this);
        dock->setObjectName("DockWidgetRoom");
        dock->setAllowedAreas(Qt::TopDockWidgetArea | Qt::BottomDockWidgetArea);
        dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable
                          | QDockWidget::DockWidgetClosable);
        addDockWidget(Qt::BottomDockWidgetArea, dock);
        dock->setWidget(w);
        dock->hide();

        m_roomWidget = w;
#endif

        m_dockDialogRoom = dock;
    });

    // Find Room Dialog
    std::invoke([this] {
#ifdef MMAPPER_WITH_QML
        m_findRoomsController = new FindRoomsController(deref(m_mapData), this);
        auto *const dialog = new QmlDialog(tr("Find Rooms"), "FindRoomsDlg", this);
        dialog->setContextProperty("findRoomsController", m_findRoomsController);
        dialog->setContextProperty("findRoomsModel", m_findRoomsController->getModel());
        dialog->setQmlSource(QUrl(QStringLiteral("qrc:/qt/qml/MMapper/FindRoomsDialog.qml")));
        m_findRoomsDlg = dialog;
#else
        auto *const findRoomsDlg = new FindRoomsDlg(deref(m_mapData), this);
        findRoomsDlg->setObjectName("FindRoomsDlg");

        m_findRoomsDlg = findRoomsDlg;
#endif
    });

    std::invoke([this] {
        auto *const adv = new AdventureTracker(deref(m_gameObserver), this);

        // View -> Side Panels -> Adventure Panel (Trophy XP, Achievements, Hints, etc)
#ifdef MMAPPER_WITH_QML
        auto *const model = new AdventureLogModel(deref(adv), this);
        auto *const dock = new QmlDockWidget(tr("Adventure Panel"), "DockWidgetGameConsole", this);
        dock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
        dock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetFloatable
                          | QDockWidget::DockWidgetMovable);
        addDockWidget(Qt::BottomDockWidgetArea, dock);
        dock->setContextProperty("adventureLogModel", model);
        dock->setQmlSource(QUrl(QStringLiteral("qrc:/qt/qml/MMapper/AdventurePanel.qml")));
        dock->hide();
#else
        auto *const w = new AdventureWidget(deref(adv), this);
        auto *const dock = new QDockWidget(tr("Adventure Panel"), this);
        dock->setObjectName("DockWidgetGameConsole");
        dock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
        dock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetFloatable
                          | QDockWidget::DockWidgetMovable);
        addDockWidget(Qt::BottomDockWidgetArea, dock);
        dock->setWidget(w);
        dock->hide();

        m_adventureWidget = w;
#endif

        m_adventureTracker = adv;
        m_dockDialogAdventure = dock;
    });

    // View -> Side Panels -> Description / Area Panel
    std::invoke([this] {
#ifdef MMAPPER_WITH_QML
        auto *const adapter = new DescriptionAdapter(deref(m_mediaLibrary), this);
        auto *const dock = new QmlDockWidget(tr("Description Panel"), "DockWidgetDescription", this);
        dock->setAllowedAreas(Qt::AllDockWidgetAreas);
        dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable
                          | QDockWidget::DockWidgetClosable);
        addDockWidget(Qt::RightDockWidgetArea, dock);
        // The engine takes ownership of the provider; it shares the image
        // store with the adapter via shared_ptr (see DescriptionImageStore.h).
        // Must be registered before setQmlSource().
        dock->addImageProvider("description", new DescriptionImageProvider(adapter->getStore()));
        dock->setContextProperty("adapter", adapter);
        dock->setQmlSource(QUrl(QStringLiteral("qrc:/qt/qml/MMapper/DescriptionPanel.qml")));

        m_descriptionAdapter = adapter;
#else
        auto *const w = new DescriptionWidget(deref(m_mediaLibrary), this);
        auto *const dock = new QDockWidget(tr("Description Panel"), this);
        dock->setObjectName("DockWidgetDescription");
        dock->setAllowedAreas(Qt::AllDockWidgetAreas);
        dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable
                          | QDockWidget::DockWidgetClosable);
        addDockWidget(Qt::RightDockWidgetArea, dock);
        dock->setWidget(w);

        m_descriptionWidget = w;
#endif
        m_dockDialogDescription = dock;
    });

    m_hotkeyManager = std::make_unique<HotkeyManager>();

    std::invoke([this] {
        auto *const timers = new CTimers(this);

#ifdef MMAPPER_WITH_QML
        auto *const model = new TimerModel(deref(timers), this);
        auto *const controller = new TimerController(deref(timers), deref(model), this);
        auto *const dock = new QmlDockWidget(tr("Timers Panel"), "DockWidgetTimers", this);
        dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable
                          | QDockWidget::DockWidgetClosable);
        dock->setContextProperty("timerModel", model);
        dock->setContextProperty("timerController", controller);
        dock->setQmlSource(QUrl(QStringLiteral("qrc:/qt/qml/MMapper/TimerPanel.qml")));
        dock->hide();
#else
        auto *const w = new TimerWidget(deref(timers), this);
        auto *const dock = new QDockWidget(tr("Timers Panel"), this);
        dock->setObjectName("DockWidgetTimers");
        dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable
                          | QDockWidget::DockWidgetClosable);
        dock->setWidget(w);
        dock->hide();

        m_timerWidget = w;
#endif

        m_timers = timers;
        m_dockDialogTimers = dock;
    });

    // Async commands
    std::invoke([this] {
#ifdef MMAPPER_WITH_QML
        auto *const model = new TasksModel(this);
        auto *const dock = new QmlDockWidget(tr("Tasks Panel"), "DockWidgetTasks", this);
        dock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
        dock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetFloatable
                          | QDockWidget::DockWidgetMovable);
        addDockWidget(Qt::BottomDockWidgetArea, dock);
        dock->setContextProperty("tasksModel", model);
        dock->setQmlSource(QUrl(QStringLiteral("qrc:/qt/qml/MMapper/TasksPanel.qml")));
        dock->hide();

        m_tasksModel = model;
#else
        auto *const w = new TasksPanel{*this};
        auto *const dock = new QDockWidget(tr("Tasks Panel"), this);
        dock->setObjectName("DockWidgetTasks");
        dock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
        dock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetFloatable
                          | QDockWidget::DockWidgetMovable);
        addDockWidget(Qt::BottomDockWidgetArea, dock);
        dock->setWidget(w);
        dock->hide();

        w->show();
#endif
        m_dockDialogAsync = dock;
    });

    m_mumeClock = new MumeClock(getConfig().mumeClock.startEpoch, deref(m_gameObserver), this);
    if constexpr (!NO_UPDATER) {
#ifdef MMAPPER_WITH_QML
        m_updateChecker = new UpdateChecker(this);
        auto *const dialog = new QmlDialog(tr("MMapper Updater"), "UpdateDialog", this);
        dialog->setContextProperty("updateChecker", m_updateChecker);
        dialog->setQmlSource(QUrl(QStringLiteral("qrc:/qt/qml/MMapper/UpdateDialog.qml")));
        connect(m_updateChecker, &UpdateChecker::sig_showDialog, dialog, [dialog]() {
            dialog->show();
            dialog->raise();
            dialog->activateWindow();
        });
        m_updateDialog = dialog;
#else
        m_updateDialog = new UpdateDialog(this);
#endif
    }

    m_logger = new AutoLogger(this);

    // TODO move this connect() wiring into AutoLogger::ctor ?
    std::invoke([this] {
        GameObserver &observer = deref(m_gameObserver);
        observer.sig2_connected.connect(m_lifetime, [this]() {
            //
            deref(m_logger).slot_onConnected();
        });
        observer.sig2_toggledEchoMode.connect(m_lifetime, [this](bool echo) {
            deref(m_logger).slot_shouldLog(echo);
        });
        observer.sig2_sentToMudString.connect(m_lifetime, [this](const QString &msg) {
            deref(m_logger).slot_writeToLog(msg);
        });
        observer.sig2_sentToUserString.connect(m_lifetime, [this](const QString &msg) {
            deref(m_logger).slot_writeToLog(msg);
        });
    });

    // View -> Side Panels -> Client Panel
    std::invoke([this] {
        auto *const timers = m_dockDialogTimers;
        std::ignore = deref(timers);

        auto *const listener = new ConnectionListener(deref(m_mapData),
                                                      deref(m_pathMachine),
                                                      deref(m_prespammedPath),
                                                      deref(m_groupManager),
                                                      deref(m_mumeClock),
                                                      deref(m_timers),
                                                      deref(getCanvas()),
                                                      deref(m_gameObserver),
                                                      this);

#ifdef MMAPPER_WITH_QML
        auto *const model = new ClientLineModel(this);
        auto *const controller = new ClientController(deref(model), deref(m_hotkeyManager), this);
        controller->setBackend(std::make_unique<ClientTelnetBackend>(deref(listener), *controller));

        auto *const dock = new QmlDockWidget(tr("Client Panel"), "DockWidgetClient", this);
        dock->setAllowedAreas(Qt::AllDockWidgetAreas);
        dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable
                          | QDockWidget::DockWidgetClosable);
        addDockWidget(Qt::RightDockWidgetArea, timers); // caution: this is different
        addDockWidget(Qt::LeftDockWidgetArea, dock);
        dock->setContextProperty("clientController", controller);
        dock->setContextProperty("clientLineModel", model);
        dock->setContextProperty("config", m_qmlConfig);
        dock->setQmlSource(QUrl(QStringLiteral("qrc:/qt/qml/MMapper/ClientPanel.qml")));

        m_listener = listener;
        m_clientLineModel = model;
        m_clientController = controller;
#else
        auto *const w = new ClientWidget(deref(listener), deref(m_hotkeyManager), this);
        w->setObjectName("InternalMudClientWidget");

        auto *const dock = new QDockWidget("Client Panel", this);
        dock->setObjectName("DockWidgetClient");
        dock->setAllowedAreas(Qt::AllDockWidgetAreas);
        dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable
                          | QDockWidget::DockWidgetClosable);
        addDockWidget(Qt::RightDockWidgetArea, timers); // caution: this is different
        addDockWidget(Qt::LeftDockWidgetArea, dock);
        dock->setWidget(w);

        m_listener = listener;
        m_clientWidget = w;
#endif
        m_dockDialogClient = dock;
    });

    createActions();
    setupToolBars();
    setupMenuBar();
    setupStatusBar();

    setCorner(Qt::TopLeftCorner, Qt::TopDockWidgetArea);
    setCorner(Qt::BottomLeftCorner, Qt::BottomDockWidgetArea);
    setCorner(Qt::TopRightCorner, Qt::TopDockWidgetArea);
    setCorner(Qt::BottomRightCorner, Qt::BottomDockWidgetArea);

    // update connections
    wireConnections();

    switch (getConfig().general.mapMode) {
    case MapModeEnum::PLAY:
        mapperMode.playModeAct->setChecked(true);
        slot_onPlayMode();
        break;
    case MapModeEnum::MAP:
        mapperMode.mapModeAct->setChecked(true);
        slot_onMapMode();
        break;
    case MapModeEnum::OFFLINE:
        mapperMode.offlineModeAct->setChecked(true);
        slot_onOfflineMode();
        break;
    }

    if (getConfig().general.alwaysOnTop) {
        alwaysOnTopAct->setChecked(true);
        setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
    }

    showStatusBarAct->setChecked(getConfig().general.showStatusBar);
    slot_setShowStatusBar();

    showScrollBarsAct->setChecked(getConfig().general.showScrollBars);
    slot_setShowScrollBars();

    if constexpr (CURRENT_PLATFORM != PlatformEnum::Mac) {
        showMenuBarAct->setChecked(getConfig().general.showMenuBar);
        slot_setShowMenuBar();
    }

    connect(m_mapData, &MapData::sig_generateBaseMap, this, &MainWindow::slot_generateBaseMap);

    readSettings();
    g_mainWindow = this;
}

void MainWindow::startServices()
{
    try {
        m_listener->listen();
        slot_log("ConnectionListener",
                 tr("Server bound on localhost to port: %1.").arg(getConfig().connection.localPort));
    } catch (const std::exception &e) {
        const QString errorMsg = QString(
                                     "Unable to start the server (switching to offline mode): %1.")
                                     .arg(QString::fromUtf8(e.what()));
        QMessageBox::critical(this, tr("mmapper"), errorMsg);
    }

    if constexpr (!NO_UPDATER) {
        auto should_check_for_update = []() -> bool { return getConfig().general.checkForUpdate; };
        // Raise the update dialog if an update is found
        if (should_check_for_update()) {
#ifdef MMAPPER_WITH_QML
            m_updateChecker->check();
#else
            m_updateDialog->open();
#endif
        }
    }
}

void MainWindow::readSettings()
{
    struct NODISCARD MySettings final
    {
#define X_DECL(name) decltype(Configuration::GeneralSettings::name) name{};
#define X_COPY(name) (my_settings.name) = (actual_settings.name);
#define XFOREACH_MY_SETTINGS(X) \
    X(firstRun) \
    X(windowGeometry) \
    X(windowState)

        // member variable declarations
        XFOREACH_MY_SETTINGS(X_DECL)

        NODISCARD static MySettings get()
        {
            // REVISIT: Does this actually need to take the lock?
            auto &&config = getConfig();
            const auto &actual_settings = config.general;
            MySettings my_settings{};
            XFOREACH_MY_SETTINGS(X_COPY)
            return my_settings;
        }

#undef X_COPY
#undef X_DECL
#undef XFOREACH_MY_SETTINGS
    };

    MySettings settings = MySettings::get();

    if (settings.firstRun) {
        if constexpr (CURRENT_PLATFORM != PlatformEnum::Wasm) {
            adjustSize();
        }
        setGeometry(QStyle::alignedRect(Qt::LeftToRight,
                                        Qt::AlignCenter,
                                        size(),
                                        qApp->primaryScreen()->availableGeometry()));

    } else {
        if (!restoreState(settings.windowState)) {
            qWarning() << "Unable to restore toolbars and dockwidgets state";
        }
        if (!restoreGeometry(settings.windowGeometry)) {
            qWarning() << "Unable to restore window geometry";
        }
        raise();

        // Check if the window was moved to a screen with a different DPI
        getCanvas()->screenChanged();
    }
}

void MainWindow::writeSettings()
{
    auto &savedConfig = setConfig().general;
    savedConfig.windowGeometry = saveGeometry();
    savedConfig.windowState = saveState();
}

void MainWindow::wireConnections()
{
    connect(m_mapData,
            &MapFrontend::sig_clearingMap,
            m_pathMachine,
            &PathMachine::slot_releaseAllPaths);

    MapCanvas *const canvas = getCanvas();
    connect(m_mapData, &MapFrontend::sig_clearingMap, canvas, &MapCanvas::slot_clearAllSelections);

    connect(m_pathMachine,
            &Mmapper2PathMachine::sig_playerMoved,
            canvas,
            &MapCanvas::slot_moveMarker);

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
            canvas,
            &MapCanvas::slot_onForcedPositionChange);

    // moved to mapwindow
    connect(m_mapData, &MapData::sig_mapSizeChanged, m_mapWindow, &MapWindow::slot_setScrollBars);

    connect(m_prespammedPath, &PrespammedPath::sig_update, canvas, &MapCanvas::slot_requestUpdate);

    connect(m_mapData, &MapData::sig_log, this, &MainWindow::slot_log);
    connect(canvas, &MapCanvas::sig_log, this, &MainWindow::slot_log);

    connect(m_mapData, &MapData::sig_onDataChanged, this, [this]() { this->updateMapModified(); });

    connect(zoomInAct, &QAction::triggered, canvas, &MapCanvas::slot_zoomIn);
    connect(zoomOutAct, &QAction::triggered, canvas, &MapCanvas::slot_zoomOut);
    connect(zoomResetAct, &QAction::triggered, canvas, &MapCanvas::slot_zoomReset);

    connect(canvas, &MapCanvas::sig_newRoomSelection, this, &MainWindow::slot_newRoomSelection);
    connect(canvas, &MapCanvas::sig_selectionChanged, this, [this]() {
        if (m_roomSelection != nullptr && m_roomSelection->size()) {
            auto anyRoomAtOffset = [this](const Coordinate offset) -> bool {
                const auto &sel = deref(m_roomSelection);
                for (const RoomId id : sel) {
                    const Coordinate here = m_mapData->getRoomHandle(id).getPosition();
                    const Coordinate target = here + offset;
                    if (m_mapData->findRoomHandle(target)) {
                        return true;
                    }
                }
                return false;
            };
            moveUpRoomSelectionAct->setEnabled(!anyRoomAtOffset(Coordinate(0, 0, 1)));
            moveDownRoomSelectionAct->setEnabled(!anyRoomAtOffset(Coordinate(0, 0, -1)));
            mergeUpRoomSelectionAct->setEnabled(anyRoomAtOffset(Coordinate(0, 0, 1)));
            mergeDownRoomSelectionAct->setEnabled(anyRoomAtOffset(Coordinate(0, 0, -1)));
        }
    });
    connect(canvas,
            &MapCanvas::sig_newConnectionSelection,
            this,
            &MainWindow::slot_newConnectionSelection);
    connect(canvas,
            &MapCanvas::sig_newInfomarkSelection,
            this,
            &MainWindow::slot_newInfomarkSelection);
    connect(canvas,
            &MapCanvas::sig_customContextMenuRequested,
            this,
            &MainWindow::slot_showContextMenu);
    connect(canvas, &MapCanvas::sig_dismissContextMenu, this, &MainWindow::slot_closeContextMenu);

    // Group
    connect(m_groupManager, &Mmapper2Group::sig_log, this, &MainWindow::slot_log);
    connect(m_groupManager,
            &Mmapper2Group::sig_updateMapCanvas,
            canvas,
            &MapCanvas::slot_requestUpdate);

#ifdef MMAPPER_WITH_QML
    connect(m_mapData,
            &MapFrontend::sig_clearingMap,
            m_groupController,
            &GroupController::slot_mapUnloaded);
#else
    connect(m_mapData, &MapFrontend::sig_clearingMap, m_groupWidget, &GroupWidget::slot_mapUnloaded);
#endif

    connect(m_mumeClock, &MumeClock::sig_log, this, &MainWindow::slot_log);

    connect(m_listener, &ConnectionListener::sig_log, this, &MainWindow::slot_log);
#ifdef MMAPPER_WITH_QML
    // Mirrors ClientWidget::slot_onVisibilityChanged() (see ClientWidget.cpp):
    // delay 500ms to distinguish a real hide/show from the dock briefly
    // popping back in, then disconnect if hidden-while-connected or focus
    // the input if shown-while-disconnected.
    connect(m_dockDialogClient, &QDockWidget::visibilityChanged, this, [this](bool /*visible*/) {
        if (!m_clientController->getUsingClient()) {
            return;
        }
        QTimer::singleShot(500, this, [this]() {
            if (m_clientController->getConnected() && !m_dockDialogClient->isVisible()) {
                m_clientController->disconnectFromMud();
            } else if (!m_clientController->getConnected() && m_dockDialogClient->isVisible()) {
                emit m_clientController->sig_requestInputFocus();
            }
        });
    });
    connect(m_listener, &ConnectionListener::sig_clientSuccessfullyConnected, this, [this]() {
        if (!m_clientController->getUsingClient()) {
            m_dockDialogClient->hide();
        }
    });
    connect(m_clientController,
            &ClientController::sig_relayMessage,
            this,
            [this](const QString &message) { showStatusShort(message); });
#else
    connect(m_dockDialogClient,
            &QDockWidget::visibilityChanged,
            m_clientWidget,
            &ClientWidget::slot_onVisibilityChanged);
    connect(m_listener, &ConnectionListener::sig_clientSuccessfullyConnected, this, [this]() {
        if (!m_clientWidget->isUsingClient()) {
            m_dockDialogClient->hide();
        }
    });
    connect(m_clientWidget, &ClientWidget::sig_relayMessage, this, [this](const QString &message) {
        showStatusShort(message);
    });
#endif

    // Find Room Dialog Connections
#ifdef MMAPPER_WITH_QML
    connect(m_findRoomsController,
            &FindRoomsController::sig_newRoomSelection,
            canvas,
            &MapCanvas::slot_setRoomSelection);
    connect(m_findRoomsController,
            &FindRoomsController::sig_center,
            m_mapWindow,
            &MapWindow::slot_centerOnWorldPos);
    connect(m_findRoomsController, &FindRoomsController::sig_log, this, &MainWindow::slot_log);
    connect(m_findRoomsController,
            &FindRoomsController::sig_editSelection,
            this,
            &MainWindow::slot_onEditRoomSelection);
    connect(m_findRoomsDlg, &QDialog::finished, this, [this](int /*result*/) {
        setConfig().findRoomsDialog.geometry = m_findRoomsDlg->saveGeometry();
    });
#else
    connect(m_findRoomsDlg,
            &FindRoomsDlg::sig_newRoomSelection,
            canvas,
            &MapCanvas::slot_setRoomSelection);
    connect(m_findRoomsDlg,
            &FindRoomsDlg::sig_center,
            m_mapWindow,
            &MapWindow::slot_centerOnWorldPos);
    connect(m_findRoomsDlg, &FindRoomsDlg::sig_log, this, &MainWindow::slot_log);
    connect(m_findRoomsDlg,
            &FindRoomsDlg::sig_editSelection,
            this,
            &MainWindow::slot_onEditRoomSelection);
#endif
}

void MainWindow::slot_log(const QString &mod, const QString &message)
{
#ifdef MMAPPER_WITH_QML
    deref(m_logModel).append(mod, message);
#else
    QTextBrowser *const logWindow = m_logWindow;
    logWindow->append(QString("[%1] %2").arg(mod, message));
    logWindow->moveCursor(QTextCursor::MoveOperation::End);
    logWindow->ensureCursorVisible();
    logWindow->update();
#endif
}

// TODO: clean up all this copy/paste by using helper functions and X-macros
void MainWindow::createActions()
{
    newAct = new QAction(QIcon::fromTheme("document-new", QIcon(":/icons/new.png")),
                         tr("&New"),
                         this);
    newAct->setStatusTip(tr("Create a new file"));
    connect(newAct, &QAction::triggered, this, &MainWindow::slot_newFile);

    openAct = new QAction(QIcon::fromTheme("document-open", QIcon(":/icons/open.png")),
                          tr("&Open..."),
                          this);
    openAct->setShortcut(tr("Ctrl+O"));
    openAct->setStatusTip(tr("Open an existing file"));
    connect(openAct, &QAction::triggered, this, &MainWindow::slot_open);

    reloadAct = new QAction(QIcon::fromTheme("document-open-recent", QIcon(":/icons/reload.png")),
                            tr("&Reload"),
                            this);
    reloadAct->setShortcut(tr("Ctrl+R"));
    reloadAct->setStatusTip(tr("Reload the current map"));
    connect(reloadAct, &QAction::triggered, this, &MainWindow::slot_reload);

    saveAct = new QAction(QIcon::fromTheme("document-save", QIcon(":/icons/save.png")),
                          tr("&Save"),
                          this);
    saveAct->setShortcut(tr("Ctrl+S"));
    saveAct->setStatusTip(tr("Save the document to disk"));
    saveAct->setEnabled(false);
    connect(saveAct, &QAction::triggered, this, &MainWindow::slot_save);

    saveAsAct = new QAction(QIcon::fromTheme("document-save-as"), tr("Save &As..."), this);
    saveAsAct->setStatusTip(tr("Save the document under a new name"));
    connect(saveAsAct, &QAction::triggered, this, &MainWindow::slot_saveAs);

    exportBaseMapAct = new QAction(tr("Export MMapper2 &Base Map As..."), this);
    exportBaseMapAct->setStatusTip(tr("Save a copy of the map with no secrets"));
    connect(exportBaseMapAct, &QAction::triggered, this, &MainWindow::slot_exportBaseMap);

    exportMm2xmlMapAct = new QAction(tr("Export MMapper2 &XML Map As..."), this);
    exportMm2xmlMapAct->setStatusTip(tr("Save a copy of the map in the MM2XML format"));
    connect(exportMm2xmlMapAct, &QAction::triggered, this, &MainWindow::slot_exportMm2xmlMap);

    exportWebMapAct = new QAction(tr("Export &Web Map As..."), this);
    exportWebMapAct->setStatusTip(tr("Save a copy of the map for webclients"));
    connect(exportWebMapAct, &QAction::triggered, this, &MainWindow::slot_exportWebMap);
    if constexpr (CURRENT_PLATFORM == PlatformEnum::Wasm) {
        exportWebMapAct->setDisabled(true);
    }

    exportMmpMapAct = new QAction(tr("Export &MMP Map As..."), this);
    exportMmpMapAct->setStatusTip(tr("Save a copy of the map in the MMP format"));
    connect(exportMmpMapAct, &QAction::triggered, this, &MainWindow::slot_exportMmpMap);

    mergeAct = new QAction(QIcon(":/icons/merge.png"), tr("&Merge..."), this);
    mergeAct->setStatusTip(tr("Merge an existing file into current map"));
    connect(mergeAct, &QAction::triggered, this, &MainWindow::slot_merge);

    exitAct = new QAction(QIcon::fromTheme("application-exit"), tr("E&xit"), this);
    exitAct->setShortcut(tr("Ctrl+Q"));
    exitAct->setStatusTip(tr("Exit the application"));
    connect(exitAct, &QAction::triggered, this, &QWidget::close);
    if constexpr (CURRENT_PLATFORM == PlatformEnum::Wasm) {
        exitAct->setDisabled(true);
    }

    m_undoAction = new QAction(QIcon::fromTheme("edit-undo"), tr("&Undo"), this);
    m_undoAction->setShortcut(QKeySequence::Undo);
    m_undoAction->setStatusTip(tr("Undo the last action"));
    connect(m_undoAction, &QAction::triggered, m_mapData, &MapData::slot_undo);
    connect(m_mapData, &MapData::sig_undoAvailable, m_undoAction, &QAction::setEnabled);
    m_undoAction->setEnabled(false);

    m_redoAction = new QAction(QIcon::fromTheme("edit-redo"), tr("&Redo"), this);
    m_redoAction->setShortcut(QKeySequence::Redo);
    m_redoAction->setStatusTip(tr("Redo the last undone action"));
    connect(m_redoAction, &QAction::triggered, m_mapData, &MapData::slot_redo);
    connect(m_mapData, &MapData::sig_redoAvailable, m_redoAction, &QAction::setEnabled);
    m_redoAction->setEnabled(false);

    preferencesAct = new QAction(QIcon::fromTheme("preferences-desktop",
                                                  QIcon(":/icons/preferences.png")),
                                 tr("&Preferences"),
                                 this);
    preferencesAct->setShortcut(tr("Ctrl+P"));
    preferencesAct->setStatusTip(tr("MMapper preferences"));
    connect(preferencesAct, &QAction::triggered, this, &MainWindow::slot_onPreferences);

    if constexpr (!NO_UPDATER) {
        mmapperCheckForUpdateAct = new QAction(QIcon(":/icons/mmapper-lo.svg"),
                                               tr("Check for &update"),
                                               this);
        connect(mmapperCheckForUpdateAct,
                &QAction::triggered,
                this,
                &MainWindow::slot_onCheckForUpdate);
    }
    mumeWebsiteAct = new QAction(tr("&Website"), this);
    connect(mumeWebsiteAct, &QAction::triggered, this, &MainWindow::slot_openMumeWebsite);
    voteAct = new QAction(QIcon::fromTheme("applications-games"), tr("V&ote for Mume"), this);
    voteAct->setStatusTip(tr("Please vote for MUME on \"The Mud Connector\""));
    connect(voteAct, &QAction::triggered, this, &MainWindow::slot_voteForMUME);
    mumeWebsiteAct = new QAction(tr("&Website"), this);
    connect(mumeWebsiteAct, &QAction::triggered, this, &MainWindow::slot_openMumeWebsite);
    mumeForumAct = new QAction(tr("&Forum"), this);
    connect(mumeForumAct, &QAction::triggered, this, &MainWindow::slot_openMumeForum);
    mumeWikiAct = new QAction(tr("W&iki"), this);
    connect(mumeWikiAct, &QAction::triggered, this, &MainWindow::slot_openMumeWiki);
    settingUpMmapperAct = new QAction(QIcon::fromTheme("help-faq"), tr("Get &Help"), this);
    connect(settingUpMmapperAct, &QAction::triggered, this, &MainWindow::slot_openSettingUpMmapper);

    actionReportIssue = new QAction(QIcon::fromTheme("help-browser"),
                                    tr("Report an &Issue..."),
                                    this);
    actionReportIssue->setStatusTip(tr("Open the MMapper issue tracker in your browser"));
    connect(actionReportIssue, &QAction::triggered, this, &MainWindow::onReportIssueTriggered);

    newbieAct = new QAction(tr("&Information for Newcomers"), this);
    newbieAct->setStatusTip("Newbie help on the MUME website");
    connect(newbieAct, &QAction::triggered, this, &MainWindow::slot_openNewbieHelp);
    aboutAct = new QAction(QIcon::fromTheme("help-about"), tr("About &MMapper"), this);
    aboutAct->setStatusTip(tr("Show the application's About box"));
    connect(aboutAct, &QAction::triggered, this, &MainWindow::slot_about);
    aboutQtAct = new QAction(tr("About &Qt"), this);
    aboutQtAct->setStatusTip(tr("Show the Qt library's About box"));
    connect(aboutQtAct, &QAction::triggered, qApp, &QApplication::aboutQt);

    zoomInAct = new QAction(QIcon::fromTheme("zoom-in", QIcon(":/icons/viewmag+.png")),
                            tr("Zoom In"),
                            this);
    zoomInAct->setStatusTip(tr("Zooms In current map"));
    zoomInAct->setShortcut(tr("Ctrl++"));
    zoomOutAct = new QAction(QIcon::fromTheme("zoom-out", QIcon(":/icons/viewmag-.png")),
                             tr("Zoom Out"),
                             this);
    zoomOutAct->setShortcut(tr("Ctrl+-"));
    zoomOutAct->setStatusTip(tr("Zooms Out current map"));
    zoomResetAct = new QAction(QIcon::fromTheme("zoom-original", QIcon(":/icons/viewmagfit.png")),
                               tr("Zoom Reset"),
                               this);
    zoomResetAct->setShortcut(tr("Ctrl+0"));
    zoomResetAct->setStatusTip(tr("Zoom to original size"));

    alwaysOnTopAct = new QAction(tr("Always On Top"), this);
    alwaysOnTopAct->setCheckable(true);
    connect(alwaysOnTopAct, &QAction::triggered, this, &MainWindow::slot_alwaysOnTop);

    showStatusBarAct = new QAction(tr("Always Show Status Bar"), this);
    showStatusBarAct->setCheckable(true);
    connect(showStatusBarAct, &QAction::triggered, this, &MainWindow::slot_setShowStatusBar);

    showScrollBarsAct = new QAction(tr("Always Show Scrollbars"), this);
    showScrollBarsAct->setCheckable(true);
    connect(showScrollBarsAct, &QAction::triggered, this, &MainWindow::slot_setShowScrollBars);

    if constexpr (CURRENT_PLATFORM != PlatformEnum::Mac) {
        showMenuBarAct = new QAction(tr("Always Show Menubar"), this);
        showMenuBarAct->setCheckable(true);
        connect(showMenuBarAct, &QAction::triggered, this, &MainWindow::slot_setShowMenuBar);
    }

    layerUpAct = new QAction(QIcon::fromTheme("go-up", QIcon(":/icons/layerup.png")),
                             tr("Layer Up"),
                             this);
    layerUpAct->setShortcut(tr(std::invoke([]() -> const char * {
        // Technically tr() could convert Ctrl to Meta, right?
        if constexpr (CURRENT_PLATFORM == PlatformEnum::Mac) {
            return "Meta+Tab";
        }
        return "Ctrl+Tab";
    })));
    layerUpAct->setStatusTip(tr("Layer Up"));
    connect(layerUpAct, &QAction::triggered, this, &MainWindow::slot_onLayerUp);
    layerDownAct = new QAction(QIcon::fromTheme("go-down", QIcon(":/icons/layerdown.png")),
                               tr("Layer Down"),
                               this);

    layerDownAct->setShortcut(tr(std::invoke([]() -> const char * {
        // Technically tr() could convert Ctrl to Meta, right?
        if constexpr (CURRENT_PLATFORM == PlatformEnum::Mac) {
            return "Meta+Shift+Tab";
        }
        return "Ctrl+Shift+Tab";
    })));
    layerDownAct->setStatusTip(tr("Layer Down"));
    connect(layerDownAct, &QAction::triggered, this, &MainWindow::slot_onLayerDown);

    layerResetAct = new QAction(QIcon::fromTheme("go-home", QIcon(":/icons/layerhome.png")),
                                tr("Layer Reset"),
                                this);
    layerResetAct->setStatusTip(tr("Layer Reset"));
    connect(layerResetAct, &QAction::triggered, this, &MainWindow::slot_onLayerReset);

    mouseMode.modeConnectionSelectAct = new QAction(QIcon(":/icons/connectionselection.png"),
                                                    tr("Select Connection"),
                                                    this);
    mouseMode.modeConnectionSelectAct->setStatusTip(tr("Select Connection"));
    mouseMode.modeConnectionSelectAct->setCheckable(true);
    connect(mouseMode.modeConnectionSelectAct,
            &QAction::triggered,
            this,
            &MainWindow::slot_onModeConnectionSelect);

    mouseMode.modeRoomRaypickAct = new QAction(QIcon(":/icons/raypick.png"),
                                               tr("Ray-pick Rooms"),
                                               this);
    mouseMode.modeRoomRaypickAct->setStatusTip(tr("Ray-pick Rooms"));
    mouseMode.modeRoomRaypickAct->setCheckable(true);
    connect(mouseMode.modeRoomRaypickAct,
            &QAction::triggered,
            this,
            &MainWindow::slot_onModeRoomRaypick);

    mouseMode.modeRoomSelectAct = new QAction(QIcon(":/icons/roomselection.png"),
                                              tr("Select Rooms"),
                                              this);
    mouseMode.modeRoomSelectAct->setStatusTip(tr("Select Rooms"));
    mouseMode.modeRoomSelectAct->setCheckable(true);
    connect(mouseMode.modeRoomSelectAct,
            &QAction::triggered,
            this,
            &MainWindow::slot_onModeRoomSelect);

    mouseMode.modeMoveSelectAct = new QAction(QIcon(":/icons/mapmove.png"), tr("Move map"), this);
    mouseMode.modeMoveSelectAct->setStatusTip(tr("Move Map"));
    mouseMode.modeMoveSelectAct->setCheckable(true);
    connect(mouseMode.modeMoveSelectAct,
            &QAction::triggered,
            this,
            &MainWindow::slot_onModeMoveSelect);
    mouseMode.modeInfomarkSelectAct = new QAction(QIcon(":/icons/infomarkselection.png"),
                                                  tr("Select Markers"),
                                                  this);
    mouseMode.modeInfomarkSelectAct->setStatusTip(tr("Select Info Markers"));
    mouseMode.modeInfomarkSelectAct->setCheckable(true);
    connect(mouseMode.modeInfomarkSelectAct,
            &QAction::triggered,
            this,
            &MainWindow::slot_onModeInfomarkSelect);
    mouseMode.modeCreateInfomarkAct = new QAction(QIcon(":/icons/infomarkcreate.png"),
                                                  tr("Create New Markers"),
                                                  this);
    mouseMode.modeCreateInfomarkAct->setStatusTip(tr("Create New Info Markers"));
    mouseMode.modeCreateInfomarkAct->setCheckable(true);
    connect(mouseMode.modeCreateInfomarkAct,
            &QAction::triggered,
            this,
            &MainWindow::slot_onModeCreateInfomarkSelect);
    mouseMode.modeCreateRoomAct = new QAction(QIcon(":/icons/roomcreate.png"),
                                              tr("Create New Rooms"),
                                              this);
    mouseMode.modeCreateRoomAct->setStatusTip(tr("Create New Rooms"));
    mouseMode.modeCreateRoomAct->setCheckable(true);
    connect(mouseMode.modeCreateRoomAct,
            &QAction::triggered,
            this,
            &MainWindow::slot_onModeCreateRoomSelect);

    mouseMode.modeCreateConnectionAct = new QAction(QIcon(":/icons/connectioncreate.png"),
                                                    tr("Create New Connection"),
                                                    this);
    mouseMode.modeCreateConnectionAct->setStatusTip(tr("Create New Connection"));
    mouseMode.modeCreateConnectionAct->setCheckable(true);
    connect(mouseMode.modeCreateConnectionAct,
            &QAction::triggered,
            this,
            &MainWindow::slot_onModeCreateConnectionSelect);

    mouseMode.modeCreateOnewayConnectionAct = new QAction(QIcon(
                                                              ":/icons/onewayconnectioncreate.png"),
                                                          tr("Create New Oneway Connection"),
                                                          this);
    mouseMode.modeCreateOnewayConnectionAct->setStatusTip(tr("Create New Oneway Connection"));
    mouseMode.modeCreateOnewayConnectionAct->setCheckable(true);
    connect(mouseMode.modeCreateOnewayConnectionAct,
            &QAction::triggered,
            this,
            &MainWindow::slot_onModeCreateOnewayConnectionSelect);

    mouseMode.mouseModeActGroup = new QActionGroup(this);
    mouseMode.mouseModeActGroup->setExclusive(true);
    mouseMode.mouseModeActGroup->addAction(mouseMode.modeMoveSelectAct);
    mouseMode.mouseModeActGroup->addAction(mouseMode.modeRoomRaypickAct);
    mouseMode.mouseModeActGroup->addAction(mouseMode.modeRoomSelectAct);
    mouseMode.mouseModeActGroup->addAction(mouseMode.modeConnectionSelectAct);
    mouseMode.mouseModeActGroup->addAction(mouseMode.modeCreateRoomAct);
    mouseMode.mouseModeActGroup->addAction(mouseMode.modeCreateConnectionAct);
    mouseMode.mouseModeActGroup->addAction(mouseMode.modeCreateOnewayConnectionAct);
    mouseMode.mouseModeActGroup->addAction(mouseMode.modeInfomarkSelectAct);
    mouseMode.mouseModeActGroup->addAction(mouseMode.modeCreateInfomarkAct);
    mouseMode.modeMoveSelectAct->setChecked(true);

    createRoomAct = new QAction(QIcon(":/icons/roomcreate.png"), tr("Create New Room"), this);
    createRoomAct->setStatusTip(tr("Create a new room under the cursor"));
    connect(createRoomAct, &QAction::triggered, this, &MainWindow::slot_onCreateRoom);

    editRoomSelectionAct = new QAction(QIcon(":/icons/roomedit.png"),
                                       tr("Edit Selected Rooms"),
                                       this);
    editRoomSelectionAct->setStatusTip(tr("Edit Selected Rooms"));
    editRoomSelectionAct->setShortcut(tr("Ctrl+E"));
    connect(editRoomSelectionAct, &QAction::triggered, this, &MainWindow::slot_onEditRoomSelection);

    deleteRoomSelectionAct = new QAction(QIcon(":/icons/roomdelete.png"),
                                         tr("Delete Selected Rooms"),
                                         this);
    deleteRoomSelectionAct->setStatusTip(tr("Delete Selected Rooms"));
    connect(deleteRoomSelectionAct,
            &QAction::triggered,
            this,
            &MainWindow::slot_onDeleteRoomSelection);

    moveUpRoomSelectionAct = new QAction(QIcon(":/icons/roommoveup.png"),
                                         tr("Move Up Selected Rooms"),
                                         this);
    moveUpRoomSelectionAct->setStatusTip(tr("Move Up Selected Rooms"));
    connect(moveUpRoomSelectionAct,
            &QAction::triggered,
            this,
            &MainWindow::slot_onMoveUpRoomSelection);
    moveDownRoomSelectionAct = new QAction(QIcon(":/icons/roommovedown.png"),
                                           tr("Move Down Selected Rooms"),
                                           this);
    moveDownRoomSelectionAct->setStatusTip(tr("Move Down Selected Rooms"));
    connect(moveDownRoomSelectionAct,
            &QAction::triggered,
            this,
            &MainWindow::slot_onMoveDownRoomSelection);
    mergeUpRoomSelectionAct = new QAction(QIcon(":/icons/roommergeup.png"),
                                          tr("Merge Up Selected Rooms"),
                                          this);
    mergeUpRoomSelectionAct->setStatusTip(tr("Merge Up Selected Rooms"));
    connect(mergeUpRoomSelectionAct,
            &QAction::triggered,
            this,
            &MainWindow::slot_onMergeUpRoomSelection);
    mergeDownRoomSelectionAct = new QAction(QIcon(":/icons/roommergedown.png"),
                                            tr("Merge Down Selected Rooms"),
                                            this);
    mergeDownRoomSelectionAct->setStatusTip(tr("Merge Down Selected Rooms"));
    connect(mergeDownRoomSelectionAct,
            &QAction::triggered,
            this,
            &MainWindow::slot_onMergeDownRoomSelection);
    connectToNeighboursRoomSelectionAct = new QAction(QIcon(":/icons/roomconnecttoneighbours.png"),
                                                      tr("Connect room(s) to its neighbour rooms"),
                                                      this);
    connectToNeighboursRoomSelectionAct->setStatusTip(tr("Connect room(s) to its neighbour rooms"));
    connect(connectToNeighboursRoomSelectionAct,
            &QAction::triggered,
            this,
            &MainWindow::slot_onConnectToNeighboursRoomSelection);

    findRoomsAct = new QAction(QIcon(":/icons/roomfind.png"), tr("&Find Rooms"), this);
    findRoomsAct->setStatusTip(tr("Find matching rooms"));
    findRoomsAct->setShortcut(tr("Ctrl+F"));
    connect(findRoomsAct, &QAction::triggered, this, &MainWindow::slot_onFindRoom);

    clientAct = new QAction(QIcon(":/icons/online.png"), tr("&Launch mud client"), this);
    clientAct->setStatusTip(tr("Launch the integrated mud client"));
    connect(clientAct, &QAction::triggered, this, &MainWindow::slot_onLaunchClient);

    saveLogAct = new QAction(QIcon::fromTheme("document-save", QIcon(":/icons/save.png")),
                             tr("Save Log as &Plain Text..."),
                             this);
#ifdef MMAPPER_WITH_QML
    // Mirrors ClientWidget::slot_saveLog() (see ClientWidget.cpp), reading
    // from ClientLineModel instead of a QTextDocument.
    connect(saveLogAct, &QAction::triggered, this, [this]() {
        const QByteArray logContent = m_clientLineModel->toPlainText().toUtf8();
        const QString newFileName = "log-"
                                    + QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss")
                                    + ".txt";
        QFileDialog::saveFileContent(logContent, newFileName);
    });
#else
    connect(saveLogAct, &QAction::triggered, m_clientWidget, &ClientWidget::slot_saveLog);
#endif
    saveLogAct->setStatusTip(tr("Save log as plain text file"));

    saveLogAsHtmlAct = new QAction(QIcon::fromTheme("document-save", QIcon(":/icons/save.png")),
                                   tr("Save Log as &HTML..."),
                                   this);
#ifdef MMAPPER_WITH_QML
    // Mirrors ClientWidget::slot_saveLogAsHtml() (see ClientWidget.cpp).
    connect(saveLogAsHtmlAct, &QAction::triggered, this, [this]() {
        const QByteArray logContent = m_clientLineModel->toHtml().toUtf8();
        const QString newFileNameHtml = "log-"
                                        + QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss")
                                        + ".html";
        QFileDialog::saveFileContent(logContent, newFileNameHtml);
    });
#else
    connect(saveLogAsHtmlAct,
            &QAction::triggered,
            m_clientWidget,
            &ClientWidget::slot_saveLogAsHtml);
#endif
    saveLogAsHtmlAct->setStatusTip(tr("Save log as HTML file"));

    releaseAllPathsAct = new QAction(QIcon(":/icons/cancel.png"), tr("Release All Paths"), this);
    releaseAllPathsAct->setStatusTip(tr("Release all paths"));
    releaseAllPathsAct->setCheckable(false);
    connect(releaseAllPathsAct,
            &QAction::triggered,
            m_pathMachine,
            &PathMachine::slot_releaseAllPaths);

    gotoRoomAct = new QAction(QIcon(":/icons/goto.png"), tr("Move to selected room"), this);
    gotoRoomAct->setStatusTip(tr("Move to selected room"));
    gotoRoomAct->setCheckable(false);
    gotoRoomAct->setEnabled(false);
    connect(gotoRoomAct, &QAction::triggered, this, [this]() {
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

    forceRoomAct = new QAction(QIcon(":/icons/force.png"),
                               tr("Update selected room with last movement"),
                               this);
    forceRoomAct->setStatusTip(tr("Update selected room with last movement"));
    forceRoomAct->setCheckable(false);
    forceRoomAct->setEnabled(false);
    connect(forceRoomAct, &QAction::triggered, this, &MainWindow::slot_forceMapperToRoom);

    selectedRoomActGroup = new QActionGroup(this);
    selectedRoomActGroup->setExclusive(false);
    selectedRoomActGroup->addAction(editRoomSelectionAct);
    selectedRoomActGroup->addAction(deleteRoomSelectionAct);
    selectedRoomActGroup->addAction(moveUpRoomSelectionAct);
    selectedRoomActGroup->addAction(moveDownRoomSelectionAct);
    selectedRoomActGroup->addAction(mergeUpRoomSelectionAct);
    selectedRoomActGroup->addAction(mergeDownRoomSelectionAct);
    selectedRoomActGroup->addAction(connectToNeighboursRoomSelectionAct);
    selectedRoomActGroup->setEnabled(false);

    deleteConnectionSelectionAct = new QAction(QIcon(":/icons/connectiondelete.png"),
                                               tr("Delete Selected Connection"),
                                               this);
    deleteConnectionSelectionAct->setStatusTip(tr("Delete Selected Connection"));
    connect(deleteConnectionSelectionAct,
            &QAction::triggered,
            this,
            &MainWindow::slot_onDeleteConnectionSelection);

    selectedConnectionActGroup = new QActionGroup(this);
    selectedConnectionActGroup->setExclusive(false);
    selectedConnectionActGroup->addAction(deleteConnectionSelectionAct);
    selectedConnectionActGroup->setEnabled(false);

    infomarkActions.editInfomarkAct = new QAction(QIcon(":/icons/infomarkedit.png"),
                                                  tr("Edit Selected Markers"),
                                                  this);
    infomarkActions.editInfomarkAct->setStatusTip(tr("Edit Selected Info Markers"));
    infomarkActions.editInfomarkAct->setCheckable(true);
    connect(infomarkActions.editInfomarkAct,
            &QAction::triggered,
            this,
            &MainWindow::slot_onEditInfomarkSelection);
    infomarkActions.deleteInfomarkAct = new QAction(QIcon(":/icons/infomarkdelete.png"),
                                                    tr("Delete Selected Markers"),
                                                    this);
    infomarkActions.deleteInfomarkAct->setStatusTip(tr("Delete Selected Info Markers"));
    infomarkActions.deleteInfomarkAct->setCheckable(true);
    connect(infomarkActions.deleteInfomarkAct,
            &QAction::triggered,
            this,
            &MainWindow::slot_onDeleteInfomarkSelection);

    infomarkActions.infomarkGroup = new QActionGroup(this);
    infomarkActions.infomarkGroup->setExclusive(false);
    infomarkActions.infomarkGroup->addAction(infomarkActions.deleteInfomarkAct);
    infomarkActions.infomarkGroup->addAction(infomarkActions.editInfomarkAct);
    infomarkActions.infomarkGroup->setEnabled(false);

    mapperMode.playModeAct = new QAction(QIcon(":/icons/online.png"),
                                         tr("Switch to play mode"),
                                         this);
    mapperMode.playModeAct->setStatusTip(tr("Switch to play mode - no new rooms are created"));
    mapperMode.playModeAct->setCheckable(true);
    connect(mapperMode.playModeAct, &QAction::triggered, this, &MainWindow::slot_onPlayMode);

    mapperMode.mapModeAct = new QAction(QIcon(":/icons/map.png"),
                                        tr("Switch to mapping mode"),
                                        this);
    mapperMode.mapModeAct->setStatusTip(
        tr("Switch to mapping mode - new rooms are created when moving"));
    mapperMode.mapModeAct->setCheckable(true);
    connect(mapperMode.mapModeAct, &QAction::triggered, this, &MainWindow::slot_onMapMode);

    mapperMode.offlineModeAct = new QAction(QIcon(":/icons/play.png"),
                                            tr("Switch to offline emulation mode"),
                                            this);
    mapperMode.offlineModeAct->setStatusTip(
        tr("Switch to offline emulation mode - you can learn areas offline"));
    mapperMode.offlineModeAct->setCheckable(true);
    connect(mapperMode.offlineModeAct, &QAction::triggered, this, &MainWindow::slot_onOfflineMode);

    mapperMode.mapModeActGroup = new QActionGroup(this);
    mapperMode.mapModeActGroup->setExclusive(true);
    mapperMode.mapModeActGroup->addAction(mapperMode.playModeAct);
    mapperMode.mapModeActGroup->addAction(mapperMode.mapModeAct);
    mapperMode.mapModeActGroup->addAction(mapperMode.offlineModeAct);
    mapperMode.mapModeActGroup->setEnabled(true);

    rebuildMeshesAct = new QAction(QIcon(":/icons/graphicscfg.png"), tr("&Rebuild World"), this);
    rebuildMeshesAct->setStatusTip(tr("Reconstruct the world mesh to fix graphical rendering bugs"));
    rebuildMeshesAct->setCheckable(false);
    connect(rebuildMeshesAct, &QAction::triggered, getCanvas(), &MapCanvas::slot_rebuildMeshes);
}

static void setConfigMapMode(const MapModeEnum mode)
{
    setConfig().general.mapMode = mode;
    getConfig().write();
}

void MainWindow::slot_onPlayMode()
{
    // map mode can only create rooms, but play mode can make changes
    setConfigMapMode(MapModeEnum::PLAY);
    modeMenu->setIcon(mapperMode.playModeAct->icon());
    // needed so that the menu updates to reflect state set by commands
    mapperMode.playModeAct->setChecked(true);
}

void MainWindow::slot_onMapMode()
{
    slot_log("MainWindow",
             "Map mode selected - new rooms are created when entering unmapped areas.");
    setConfigMapMode(MapModeEnum::MAP);
    modeMenu->setIcon(mapperMode.mapModeAct->icon());
    // needed so that the menu updates to reflect state set by commands
    mapperMode.mapModeAct->setChecked(true);
}

void MainWindow::slot_onOfflineMode()
{
    slot_log("MainWindow", "Offline emulation mode selected - learn new areas safely.");
    setConfigMapMode(MapModeEnum::OFFLINE);
    modeMenu->setIcon(mapperMode.offlineModeAct->icon());
    // needed so that the menu updates to reflect state set by commands
    mapperMode.offlineModeAct->setChecked(true);
}

void MainWindow::slot_setMode(MapModeEnum mode)
{
    switch (mode) {
    case MapModeEnum::PLAY:
        slot_onPlayMode();
        break;
    case MapModeEnum::MAP:
        slot_onMapMode();
        break;
    case MapModeEnum::OFFLINE:
        slot_onOfflineMode();
        break;
    }
}

void MainWindow::disableActions(bool value)
{
    if constexpr (IS_DEBUG_BUILD) {
        qDebug() << "disableActions" << value;
    }

    // REVISIT: Which of these should be allowed during async actions?
    // Note: Some of the ones that would launch async actions (e.g. loading/saving)
    // would probably be blocked because we only allow one async action at a time,
    // but they'll probably need to be individually tested to see what actually happens.
    newAct->setDisabled(value);
    openAct->setDisabled(value);
    mergeAct->setDisabled(value);
    reloadAct->setDisabled(value);
    saveAsAct->setDisabled(value);
    exportBaseMapAct->setDisabled(value);
    exportMm2xmlMapAct->setDisabled(value);
    if constexpr (CURRENT_PLATFORM != PlatformEnum::Wasm) {
        exportWebMapAct->setDisabled(value);
        exitAct->setDisabled(value);
    }
    exportMmpMapAct->setDisabled(value);
}

void MainWindow::hideCanvas(const bool hide)
{
    // REVISIT: It seems that updates don't work if the canvas is hidden,
    // so we may want to save mapChanged() and other similar requests
    // and send them after we show the canvas.
    if (MapCanvas *const canvas = getCanvas()) {
        if (hide) {
            canvas->hide();
        } else {
            canvas->show();
        }
    }
}

void MainWindow::setupMenuBar()
{
    fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(newAct);
    fileMenu->addAction(openAct);
    fileMenu->addAction(saveAct);
    if constexpr (CURRENT_PLATFORM != PlatformEnum::Wasm) {
        fileMenu->addAction(saveAsAct);
        fileMenu->addAction(reloadAct);
    }
    fileMenu->addSeparator();
    QMenu *exportMenu = fileMenu->addMenu(QIcon::fromTheme("document-send"), tr("&Export"));
    exportMenu->addAction(exportBaseMapAct);
    exportMenu->addAction(exportMm2xmlMapAct);
    exportMenu->addAction(exportWebMapAct);
    exportMenu->addAction(exportMmpMapAct);
    fileMenu->addAction(mergeAct);
    if constexpr (CURRENT_PLATFORM != PlatformEnum::Wasm) {
        fileMenu->addSeparator();
        fileMenu->addAction(exitAct);
    }

    editMenu = menuBar()->addMenu(tr("&Edit"));
    modeMenu = editMenu->addMenu(QIcon(":/icons/online.png"), tr("&Mode"));
    modeMenu->addAction(mapperMode.playModeAct);
    modeMenu->addAction(mapperMode.mapModeAct);
    modeMenu->addAction(mapperMode.offlineModeAct);
    editMenu->addSeparator();
    editMenu->addAction(m_undoAction);
    editMenu->addAction(m_redoAction);
    editMenu->addSeparator();

    QMenu *infoMarkMenu = editMenu->addMenu(QIcon(":/icons/infomarkselection.png"), tr("M&arkers"));
    infoMarkMenu->setStatusTip("Info markers");
    infoMarkMenu->addAction(mouseMode.modeInfomarkSelectAct);
    infoMarkMenu->addSeparator();
    infoMarkMenu->addAction(mouseMode.modeCreateInfomarkAct);
    infoMarkMenu->addAction(infomarkActions.editInfomarkAct);
    infoMarkMenu->addAction(infomarkActions.deleteInfomarkAct);

    roomMenu = editMenu->addMenu(QIcon(":/icons/roomselection.png"), tr("&Rooms"));
    roomMenu->addAction(mouseMode.modeRoomSelectAct);
    roomMenu->addAction(mouseMode.modeRoomRaypickAct);
    roomMenu->addSeparator();
    roomMenu->addAction(mouseMode.modeCreateRoomAct);
    roomMenu->addAction(editRoomSelectionAct);
    roomMenu->addAction(deleteRoomSelectionAct);
    roomMenu->addAction(moveUpRoomSelectionAct);
    roomMenu->addAction(moveDownRoomSelectionAct);
    roomMenu->addAction(mergeUpRoomSelectionAct);
    roomMenu->addAction(mergeDownRoomSelectionAct);
    roomMenu->addAction(connectToNeighboursRoomSelectionAct);

    connectionMenu = editMenu->addMenu(QIcon(":/icons/connectionselection.png"), tr("&Connections"));
    connectionMenu->addAction(mouseMode.modeConnectionSelectAct);
    connectionMenu->addSeparator();
    connectionMenu->addAction(mouseMode.modeCreateConnectionAct);
    connectionMenu->addAction(mouseMode.modeCreateOnewayConnectionAct);
    connectionMenu->addAction(deleteConnectionSelectionAct);

    editMenu->addSeparator();
    editMenu->addAction(findRoomsAct);
    editMenu->addAction(preferencesAct);

    viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(mouseMode.modeMoveSelectAct);
    QMenu *toolbars = viewMenu->addMenu(tr("&Toolbars"));
    toolbars->addAction(fileToolBar->toggleViewAction());
    toolbars->addAction(mapperModeToolBar->toggleViewAction());
    toolbars->addAction(mouseModeToolBar->toggleViewAction());
    toolbars->addAction(viewToolBar->toggleViewAction());
    toolbars->addAction(pathMachineToolBar->toggleViewAction());
    toolbars->addAction(roomToolBar->toggleViewAction());
    toolbars->addAction(connectionToolBar->toggleViewAction());
    toolbars->addAction(settingsToolBar->toggleViewAction());
    toolbars->addAction(audioToolBar->toggleViewAction());
    QMenu *sidepanels = viewMenu->addMenu(tr("&Side Panels"));
    sidepanels->addAction(m_dockDialogLog->toggleViewAction());
    sidepanels->addAction(m_dockDialogClient->toggleViewAction());
    sidepanels->addAction(m_dockDialogGroup->toggleViewAction());
    sidepanels->addAction(m_dockDialogRoom->toggleViewAction());
    sidepanels->addAction(m_dockDialogAdventure->toggleViewAction());
    sidepanels->addAction(m_dockDialogDescription->toggleViewAction());
    sidepanels->addAction(m_dockDialogTimers->toggleViewAction());
    sidepanels->addAction(m_dockDialogAsync->toggleViewAction());
    viewMenu->addSeparator();
    viewMenu->addAction(zoomInAct);
    viewMenu->addAction(zoomOutAct);
    viewMenu->addAction(zoomResetAct);
    viewMenu->addSeparator();
    viewMenu->addAction(layerUpAct);
    viewMenu->addAction(layerDownAct);
    viewMenu->addAction(layerResetAct);
    viewMenu->addSeparator();
    viewMenu->addAction(rebuildMeshesAct);
    viewMenu->addSeparator();
    viewMenu->addAction(showStatusBarAct);
    viewMenu->addAction(showScrollBarsAct);
    if constexpr (CURRENT_PLATFORM != PlatformEnum::Mac) {
        viewMenu->addAction(showMenuBarAct);
    }
    viewMenu->addAction(alwaysOnTopAct);

    settingsMenu = menuBar()->addMenu(tr("&Tools"));
    QMenu *clientMenu = settingsMenu->addMenu(QIcon(":/icons/terminal.png"),
                                              tr("&Integrated Mud Client"));
    clientMenu->addAction(clientAct);
    clientMenu->addAction(saveLogAct);
    clientMenu->addAction(saveLogAsHtmlAct);
    QMenu *pathMachineMenu = settingsMenu->addMenu(QIcon(":/icons/goto.png"), tr("&Path Machine"));
    pathMachineMenu->addAction(mouseMode.modeRoomSelectAct);
    pathMachineMenu->addSeparator();
    pathMachineMenu->addAction(gotoRoomAct);
    pathMachineMenu->addAction(forceRoomAct);
    pathMachineMenu->addAction(releaseAllPathsAct);

    helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(settingUpMmapperAct);
    helpMenu->addAction(actionReportIssue);
    if constexpr (!NO_UPDATER) {
        helpMenu->addAction(mmapperCheckForUpdateAct);
    }
    helpMenu->addSeparator();
    mumeMenu = helpMenu->addMenu(QIcon::fromTheme("help-contents"), tr("M&UME"));
    mumeMenu->addAction(voteAct);
    mumeMenu->addAction(newbieAct);
    mumeMenu->addAction(mumeWebsiteAct);
    mumeMenu->addAction(mumeForumAct);
    mumeMenu->addAction(mumeWikiAct);
    helpMenu->addSeparator();
    helpMenu->addAction(aboutAct);
    if constexpr (CURRENT_PLATFORM != PlatformEnum::Wasm) {
        helpMenu->addAction(aboutQtAct);
    }
}

void MainWindow::slot_showContextMenu(const QPoint &pos)
{
    slot_closeContextMenu();

    m_contextMenu = new QMenu(tr("Context menu"), this);
    auto &contextMenu = deref(m_contextMenu);
    contextMenu.setAttribute(Qt::WA_DeleteOnClose);
    if (m_connectionSelection != nullptr) {
        // Connections cannot be selected alongside rooms and infomarks
        // ^^^ Let's enforce that with a variant then?
        contextMenu.addAction(deleteConnectionSelectionAct);

    } else {
        // However, both rooms and infomarks can be selected at once
        if (m_roomSelection != nullptr) {
            if (m_roomSelection->empty()) {
                contextMenu.addAction(createRoomAct);
            } else {
                contextMenu.addAction(editRoomSelectionAct);
                contextMenu.addAction(moveUpRoomSelectionAct);
                contextMenu.addAction(moveDownRoomSelectionAct);
                contextMenu.addAction(mergeUpRoomSelectionAct);
                contextMenu.addAction(mergeDownRoomSelectionAct);
                contextMenu.addAction(deleteRoomSelectionAct);
                contextMenu.addAction(connectToNeighboursRoomSelectionAct);
                contextMenu.addSeparator();
                contextMenu.addAction(gotoRoomAct);
                contextMenu.addAction(forceRoomAct);
            }
        }
        if (m_infoMarkSelection != nullptr && !m_infoMarkSelection->empty()) {
            if (m_roomSelection != nullptr) {
                contextMenu.addSeparator();
            }
            contextMenu.addAction(infomarkActions.editInfomarkAct);
            contextMenu.addAction(infomarkActions.deleteInfomarkAct);
        }
    }
    contextMenu.addSeparator();
    QMenu *mouseMenu = contextMenu.addMenu(QIcon::fromTheme("input-mouse"), "Mouse Mode");
    mouseMenu->addAction(mouseMode.modeMoveSelectAct);
    mouseMenu->addAction(mouseMode.modeRoomRaypickAct);
    mouseMenu->addAction(mouseMode.modeRoomSelectAct);
    mouseMenu->addAction(mouseMode.modeInfomarkSelectAct);
    mouseMenu->addAction(mouseMode.modeConnectionSelectAct);
    mouseMenu->addAction(mouseMode.modeCreateInfomarkAct);
    mouseMenu->addAction(mouseMode.modeCreateRoomAct);
    mouseMenu->addAction(mouseMode.modeCreateConnectionAct);
    mouseMenu->addAction(mouseMode.modeCreateOnewayConnectionAct);

    contextMenu.popup(getCanvas()->mapToGlobal(pos));
}

void MainWindow::slot_closeContextMenu()
{
    if (m_contextMenu) {
        m_contextMenu->close();
    }
}

void MainWindow::slot_alwaysOnTop()
{
    const bool alwaysOnTop = this->alwaysOnTopAct->isChecked();
    qInfo() << "Setting AlwaysOnTop flag to " << alwaysOnTop;
    setWindowFlag(Qt::WindowStaysOnTopHint, alwaysOnTop);
    setConfig().general.alwaysOnTop = alwaysOnTop;
    show();
}

void MainWindow::slot_setShowStatusBar()
{
    const bool showStatusBar = this->showStatusBarAct->isChecked();
    statusBar()->setVisible(showStatusBar);
    setConfig().general.showStatusBar = showStatusBar;
    show();
}

void MainWindow::slot_setShowScrollBars()
{
    const bool showScrollBars = this->showScrollBarsAct->isChecked();
    setConfig().general.showScrollBars = showScrollBars;
    m_mapWindow->updateScrollBars();
    show();
}

void MainWindow::slot_setShowMenuBar()
{
    const bool showMenuBar = this->showMenuBarAct->isChecked();
    setConfig().general.showMenuBar = showMenuBar;
    show();

    m_dockDialogAdventure->setMouseTracking(!showMenuBar);
    m_dockDialogClient->setMouseTracking(!showMenuBar);
    m_dockDialogGroup->setMouseTracking(!showMenuBar);
    m_dockDialogLog->setMouseTracking(!showMenuBar);
    m_dockDialogRoom->setMouseTracking(!showMenuBar);

    if (showMenuBar) {
        menuBar()->show();
        m_dockDialogAdventure->removeEventFilter(this);
        m_dockDialogClient->removeEventFilter(this);
        m_dockDialogGroup->removeEventFilter(this);
        m_dockDialogLog->removeEventFilter(this);
        m_dockDialogRoom->removeEventFilter(this);
        m_mapWindow->removeEventFilter(this);
        m_mapWindow->getCanvas()->removeEventFilter(this);
    } else {
        m_dockDialogAdventure->installEventFilter(this);
        m_dockDialogClient->installEventFilter(this);
        m_dockDialogGroup->installEventFilter(this);
        m_dockDialogLog->installEventFilter(this);
        m_dockDialogRoom->installEventFilter(this);
        m_mapWindow->installEventFilter(this);
        m_mapWindow->getCanvas()->installEventFilter(this);
    }
}

void MainWindow::setupToolBars()
{
    fileToolBar = addToolBar(tr("File"));
    fileToolBar->setObjectName("FileToolBar");
    fileToolBar->addAction(newAct);
    fileToolBar->addAction(openAct);
    fileToolBar->addAction(saveAct);
    fileToolBar->hide();

    mapperModeToolBar = addToolBar(tr("Mapper Mode"));
    mapperModeToolBar->setObjectName("MapperModeToolBar");
    mapperModeToolBar->addAction(mapperMode.playModeAct);
    mapperModeToolBar->addAction(mapperMode.mapModeAct);
    mapperModeToolBar->addAction(mapperMode.offlineModeAct);
    mapperModeToolBar->hide();

    mouseModeToolBar = addToolBar(tr("Mouse Mode"));
    mouseModeToolBar->setObjectName("ModeToolBar");
    mouseModeToolBar->addAction(mouseMode.modeMoveSelectAct);
    mouseModeToolBar->addAction(mouseMode.modeRoomRaypickAct);
    mouseModeToolBar->addAction(mouseMode.modeRoomSelectAct);
    mouseModeToolBar->addAction(mouseMode.modeConnectionSelectAct);
    mouseModeToolBar->addAction(mouseMode.modeCreateRoomAct);
    mouseModeToolBar->addAction(mouseMode.modeCreateConnectionAct);
    mouseModeToolBar->addAction(mouseMode.modeCreateOnewayConnectionAct);
    mouseModeToolBar->addAction(mouseMode.modeInfomarkSelectAct);
    mouseModeToolBar->addAction(mouseMode.modeCreateInfomarkAct);
    mouseModeToolBar->hide();

    viewToolBar = addToolBar(tr("View"));
    viewToolBar->setObjectName("ViewToolBar");
    viewToolBar->addAction(zoomInAct);
    viewToolBar->addAction(zoomOutAct);
    viewToolBar->addAction(zoomResetAct);
    viewToolBar->addWidget(new MapZoomSlider(deref(m_mapWindow), this));
    viewToolBar->addAction(layerUpAct);
    viewToolBar->addAction(layerDownAct);
    viewToolBar->addAction(layerResetAct);
    viewToolBar->hide();

    pathMachineToolBar = addToolBar(tr("Path Machine"));
    pathMachineToolBar->setObjectName("PathMachineToolBar");
    pathMachineToolBar->addAction(releaseAllPathsAct);
    pathMachineToolBar->addAction(gotoRoomAct);
    pathMachineToolBar->addAction(forceRoomAct);
    pathMachineToolBar->hide();

    roomToolBar = addToolBar(tr("Rooms"));
    roomToolBar->setObjectName("RoomsToolBar");
    roomToolBar->addAction(findRoomsAct);
    roomToolBar->addAction(editRoomSelectionAct);
    roomToolBar->addAction(deleteRoomSelectionAct);
    roomToolBar->addAction(moveUpRoomSelectionAct);
    roomToolBar->addAction(moveDownRoomSelectionAct);
    roomToolBar->addAction(mergeUpRoomSelectionAct);
    roomToolBar->addAction(mergeDownRoomSelectionAct);
    roomToolBar->addAction(connectToNeighboursRoomSelectionAct);
    roomToolBar->hide();

    connectionToolBar = addToolBar(tr("Connections"));
    connectionToolBar->setObjectName("ConnectionsToolBar");
    connectionToolBar->addAction(deleteConnectionSelectionAct);
    connectionToolBar->hide();

    settingsToolBar = addToolBar(tr("Preferences"));
    settingsToolBar->setObjectName("PreferencesToolBar");
    settingsToolBar->addAction(preferencesAct);
    settingsToolBar->hide();

    audioToolBar = addToolBar(tr("Audio"));
    audioToolBar->setObjectName("AudioToolBar");
    audioToolBar->addWidget(
        new AudioVolumeSlider(AudioVolumeSlider::AudioType::Music, audioToolBar));
    audioToolBar->addSeparator();
    audioToolBar->addWidget(
        new AudioVolumeSlider(AudioVolumeSlider::AudioType::Sound, audioToolBar));
    audioToolBar->hide();
}

void MainWindow::setupStatusBar()
{
    showStatusForever(tr("Say friend and enter..."));

#ifdef MMAPPER_WITH_QML
    // These are simple always-on status bar widgets, not dockable panels, so
    // they use a bare QQuickWidget directly rather than QmlDockWidget (which
    // adds QDockWidget machinery these don't need).
    auto *const clockAdapter = new ClockAdapter(deref(m_gameObserver), deref(m_mumeClock), this);
    auto *const clockQuick = new QQuickWidget(statusBar());
    clockQuick->setResizeMode(QQuickWidget::SizeViewToRootObject);
    // Let the status bar show through the item's transparent background
    // instead of painting an opaque QQuickWidget backing rectangle behind it.
    clockQuick->setClearColor(Qt::transparent);
    clockQuick->setAttribute(Qt::WA_TranslucentBackground);
    // Must be registered before setSource() below: the root context must have
    // all properties before the root object is instantiated (see
    // QmlDockWidget::setQmlSource()'s comment for the same rule).
    clockQuick->rootContext()->setContextProperty("clock", clockAdapter);
    clockQuick->setSource(QUrl(QStringLiteral("qrc:/qt/qml/MMapper/ClockStrip.qml")));
    statusBar()->insertPermanentWidget(0, clockQuick);
    // showToolTip() needs the hosting widget to map the QML scene
    // coordinates ClockStrip.qml reports into global screen coordinates
    // (see ClockAdapter::showToolTip()); clockQuick must already exist,
    // hence this can't happen in ClockAdapter's own constructor.
    clockAdapter->setToolTipWidget(clockQuick);

    m_clockAdapter = clockAdapter;

    auto *const xpAdapter = new XpStatusAdapter(deref(m_adventureTracker), this);
    auto *const xpQuick = new QQuickWidget(statusBar());
    xpQuick->setResizeMode(QQuickWidget::SizeViewToRootObject);
    xpQuick->setClearColor(Qt::transparent);
    xpQuick->setAttribute(Qt::WA_TranslucentBackground);
    xpQuick->setToolTip(tr("Click to toggle the Adventure Panel."));
    xpQuick->rootContext()->setContextProperty("adapter", xpAdapter);
    xpQuick->setSource(QUrl(QStringLiteral("qrc:/qt/qml/MMapper/XpStatusItem.qml")));
    statusBar()->insertPermanentWidget(0, xpQuick);

    connect(xpAdapter, &XpStatusAdapter::sig_showStatusMessage, this, [this](const QString &msg) {
        statusBar()->showMessage(msg);
    });
    connect(xpAdapter, &XpStatusAdapter::sig_clearStatusMessage, this, [this]() {
        statusBar()->clearMessage();
    });
    connect(xpAdapter, &XpStatusAdapter::sig_toggleAdventurePanel, this, [this]() {
        auto &dock = deref(m_dockDialogAdventure);
        dock.setVisible(!dock.isVisible());
    });

    m_xpStatusAdapter = xpAdapter;
#else
    statusBar()->insertPermanentWidget(0,
                                       new MumeClockWidget(deref(m_gameObserver),
                                                           deref(m_mumeClock),
                                                           this));

    auto *const xpStatus = new XPStatusWidget(deref(m_adventureTracker), statusBar(), this);
    xpStatus->setToolTip("Click to toggle the Adventure Panel.");

    connect(xpStatus, &QPushButton::clicked, [this]() {
        auto &dock = deref(m_dockDialogAdventure);
        dock.setVisible(!dock.isVisible());
    });
    statusBar()->insertPermanentWidget(0, xpStatus);
#endif

    auto *const pathmachineStatus = new QLabel(statusBar());
    connect(m_pathMachine, &Mmapper2PathMachine::sig_state, pathmachineStatus, &QLabel::setText);
    statusBar()->insertPermanentWidget(0, pathmachineStatus);
}

#ifdef MMAPPER_WITH_QML
namespace { // anonymous
// Parity with ConfigDialog::closeEvent(): closing the preferences window via
// the title bar persists the (apply-on-change) settings to disk. Only the
// explicit Cancel button reverts; Esc merely hides the dialog, exactly like
// the widget (whose button-box, not the dialog, owned the revert path).
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
} // namespace
#endif

void MainWindow::slot_onPreferences()
{
#ifdef MMAPPER_WITH_QML
    if (m_preferencesDialog == nullptr) {
        std::ignore = deref(m_mapWindow);
        std::ignore = deref(m_groupManager);

        auto *const controller = new PreferencesController(this, this);
        auto *const dialog = new PreferencesQmlDialog(tr("Preferences"), "ConfigDialog", this);
        dialog->setContextProperty("preferencesController", controller);
        dialog->setQmlSource(QUrl(QStringLiteral("qrc:/qt/qml/MMapper/PreferencesDialog.qml")));

        // See ParserPageAdapter.h's ColorPicker doc comment for why this
        // can't be wired up inside ParserPageAdapter.cpp itself.
        deref(controller->getParser())
            .setColorPicker([this](const QString &ansiString,
                                   QWidget *const colorDialogParent,
                                   std::function<void(QString)> callback) {
                ansi_color_picker::getColor(ansiString, colorDialogParent, std::move(callback));
            });

        connect(controller,
                &PreferencesController::sig_graphicsSettingsChanged,
                m_mapWindow,
                &MapWindow::slot_graphicsSettingsChanged);
        connect(controller,
                &PreferencesController::sig_groupSettingsChanged,
                m_groupManager,
                &Mmapper2Group::slot_groupSettingsChanged);
        // GroupManagerSettings has no ChangeMonitor (see QmlConfig.h), so
        // QmlConfig cannot observe the controller's writes on its own;
        // re-sync it explicitly whenever the dialog reports a group settings
        // change or closes (mirrors the widget ConfigDialog's equivalent
        // wiring; see git history for src/mainwindow/mainwindow.cpp).
        connect(controller,
                &PreferencesController::sig_groupSettingsChanged,
                m_qmlConfig,
                &QmlConfig::reload);
        connect(dialog, &QDialog::finished, this, [this](MAYBE_UNUSED int result) {
            m_qmlConfig->reload();
            // Likewise, the Description panel's colors/font (parser.roomName/
            // DescColor, integratedClient.font/colors) have no ChangeMonitor;
            // re-resolve them after the dialog closes.
            deref(m_descriptionAdapter).reloadConfig();
        });

        m_preferencesController = controller;
        m_preferencesDialog = dialog;
    }

    // Mirrors ConfigDialog::showEvent()'s sig_loadConfig() fan-out: re-sync
    // every adapter against the live Configuration each time the dialog is
    // (re)shown, since the controller/dialog persist across opens instead of
    // being recreated per-open like the widget ConfigDialog was.
    deref(m_preferencesController).reloadAll();

    auto &dialog = deref(m_preferencesDialog);
    dialog.show();
    dialog.raise();
    dialog.activateWindow();
#else
    if (m_configDialog == nullptr) {
        auto unique_configDialog = std::make_unique<ConfigDialog>(this);
        auto *const configDialog = unique_configDialog.get();

        std::ignore = deref(configDialog);
        std::ignore = deref(m_mapWindow);
        std::ignore = deref(m_groupManager);

        connect(configDialog,
                &ConfigDialog::sig_graphicsSettingsChanged,
                m_mapWindow,
                &MapWindow::slot_graphicsSettingsChanged);
        connect(configDialog,
                &ConfigDialog::sig_groupSettingsChanged,
                m_groupManager,
                &Mmapper2Group::slot_groupSettingsChanged);
        connect(configDialog, &QDialog::finished, this, [this](MAYBE_UNUSED int result) {
            m_configDialog.reset();
        });

        m_configDialog = std::move(unique_configDialog);
    }

    auto &configDialog = deref(m_configDialog);
    configDialog.show();
    configDialog.raise();
    configDialog.activateWindow();
#endif
}

void MainWindow::slot_newRoomSelection(const SigRoomSelection &rs)
{
    const bool isValidSelection = rs.isValid();
    const size_t selSize = !isValidSelection ? 0 : rs.deref().size();

    m_roomSelection = !isValidSelection ? nullptr : rs.getShared();
    selectedRoomActGroup->setEnabled(isValidSelection);
    gotoRoomAct->setEnabled(selSize == 1);
    forceRoomAct->setEnabled(selSize == 1 && m_pathMachine->hasLastEvent());

    if (isValidSelection) {
        const auto msg = QString("Selection: %1 room%2").arg(selSize).arg(basic_plural(selSize));
        showStatusLong(msg);
    }
}

void MainWindow::slot_newConnectionSelection(ConnectionSelection *const cs)
{
    m_connectionSelection = (cs != nullptr) ? cs->shared_from_this() : nullptr;
    selectedConnectionActGroup->setEnabled(m_connectionSelection != nullptr);
}

void MainWindow::slot_newInfomarkSelection(InfomarkSelection *const is)
{
    const bool isNonNull = (is != nullptr);
    m_infoMarkSelection = isNonNull ? is->shared_from_this() : nullptr;
    deref(infomarkActions.infomarkGroup).setEnabled(isNonNull);

    if (isNonNull) {
        auto &ref = *is;
        showStatusLong(
            QString("Selection: %1 mark%2").arg(ref.size()).arg((ref.size() != 1) ? "s" : ""));
        if (ref.empty()) {
            // Create a new infomark if its an empty selection
            slot_onEditInfomarkSelection();
        }
    }
}

bool MainWindow::eventFilter(QObject *const obj, QEvent *const event)
{
    if (QApplication::activeWindow() == this && event->type() == QEvent::MouseMove) {
        if (const auto *const mouseEvent = dynamic_cast<QMouseEvent *>(event)) {
            QRect rect = geometry();
            rect.setHeight(menuBar()->sizeHint().height());
            if (rect.contains(mouseEvent->globalPosition().toPoint())) {
                menuBar()->show();
            } else {
                menuBar()->hide();
            }
        }
    }
    return QObject::eventFilter(obj, event);
}

void MainWindow::closeEvent(QCloseEvent *const event)
{
    qInfo() << MM_SOURCE_LOCATION().function_name();

    auto &asyncIO = getAsyncIO();

    if (asyncIO.isRunningOnBackgroundThread()) {
        // first check avoids prompting to save while saving.
        if (!asyncIO.is_allowed_to_cancel()) {
            qInfo() << "Note: Ignoring close request because the current async task"
                    << asyncIO.getTaskNameAsQString() << "cannot be canceled.";
            event->ignore();
            return;
        }
    }

    if (!asyncIO.isClosedForBusiness() && !asyncIO.isWaitingForSaveAtShutdown() && !maybeSave()) {
        event->ignore();
        return;
    }

    asyncIO.setClosedForBusiness();
    async_tasks::cancel_all(); /* note: this can only cancel tasks that allow it */

    if (asyncIO.isRunningOnBackgroundThread()) {
        // second check is in case we just scheduled a save.
        if (!asyncIO.is_allowed_to_cancel()) {
            asyncIO.setWaitingForSaveAtShutdown();
            qInfo() << "Note: Ignoring close request because the scheduled async task" << asyncIO
                    << "cannot be canceled.";
            event->ignore();
            hide();
            setEnabled(false);
            return;
        }

        // otherwise...

        {
            qInfo() << "Attempting to cancel async task" << asyncIO << "for faster shutdown";
            asyncIO.request_cancel();
        }

        if (auto dlg = asyncIO.getProgressDlg()) {
            qInfo() << "Attempting to reject the progress dialog for faster shutdown";
            dlg->reject();
        }
    }

    // calling hide() here prevents any decendents of mainwindow from triggering UI updates,
    // which means AsyncTaskListWidget won't query async_tasks after async_tasks::cleanup().
    hide();
    async_tasks::cleanup(); /* note: this waits */
    writeSettings();

    if (auto canvas = getCanvas()) {
        canvas->shuttingDown();
    }

    event->accept();
}

void MainWindow::showEvent(QShowEvent *const event)
{
    // Check screen DPI each time MMapper's window is shown
    getCanvas()->screenChanged();

    static std::once_flag flag;
    std::call_once(flag, [this]() {
        // Start services on startup
        startServices();

        connect(window()->windowHandle(), &QWindow::screenChanged, this, [this]() {
            MapWindow &window = deref(m_mapWindow);
            CanvasDisabler canvasDisabler{window};
            MapCanvas &canvas = deref(getCanvas());
            canvas.screenChanged();
        });
    });
    event->accept();
}

void MainWindow::slot_newFile()
{
    if (maybeSave()) {
        forceNewFile();
    }
}

void MainWindow::updateDescriptionRoom(const RoomHandle &room)
{
#ifdef MMAPPER_WITH_QML
    deref(m_descriptionAdapter).updateRoom(room);
#else
    deref(m_descriptionWidget).updateRoom(room);
#endif
}

void MainWindow::forceNewFile()
{
    {
        auto &mapData = deref(m_mapData);
        mapData.clear();
        mapData.setFileName("", false);
        mapData.forcePosition(Coordinate{});
    }

    setCurrentFile("");
    getCanvas()->slot_dataLoaded();
#ifdef MMAPPER_WITH_QML
    m_groupController->slot_mapLoaded();
#else
    m_groupWidget->slot_mapLoaded();
#endif
    updateDescriptionRoom(RoomHandle{});
    m_audioManager->onAreaChanged(RoomArea{});

    /*
    updateMapModified();
    mapChanged();
    */
}

void MainWindow::slot_open()
{
    if (!maybeSave()) {
        return;
    }

    auto openFile = [this](const QString &fileName, std::optional<QByteArray> fileContent) {
        if (fileName.isEmpty()) {
            showStatusShort(tr("No filename provided"));
            return;
        }

        try {
            loadFile(MapSource::alloc(fileName, fileContent));

            QFileInfo file(fileName);
            auto &savedLastMapDir = setConfig().autoLoad.lastMapDirectory;
            savedLastMapDir = file.dir().absolutePath();
        } catch (const std::runtime_error &e) {
            showWarning(tr("Cannot open file %1:\n%2.").arg(fileName, e.what()));
            return;
        }
    };
    const auto nameFilter = QStringLiteral("MMapper2 maps (*.mm2)"
                                           ";;MMapper2 XML or Pandora maps (*.xml)"
                                           ";;Alternate suffix for MMapper2 XML maps (*.mm2xml)");
    if constexpr (CURRENT_PLATFORM == PlatformEnum::Wasm) {
        QFileDialog::getOpenFileContent(nameFilter, openFile);
    } else {
        const QString &savedLastMapDir = setConfig().autoLoad.lastMapDirectory;
        const QString fileName = QFileDialog::getOpenFileName(this,
                                                              "Choose map file ...",
                                                              savedLastMapDir,
                                                              nameFilter);
        openFile(fileName, std::nullopt);
    }
}

void MainWindow::slot_reload()
{
    if (maybeSave()) {
        // make a copy of the filename, since it will be modified by loadFile().
        const QString filename = m_mapData->getFileName();
        try {
            loadFile(MapSource::alloc(filename));
        } catch (const std::runtime_error &e) {
            showWarning(tr("Cannot open file %1:\n%2.").arg(filename, e.what()));
            return;
        }
    }
}

void MainWindow::slot_about()
{
#ifdef MMAPPER_WITH_QML
    auto *const dialog = new QmlDialog(tr("About MMapper"), "AboutDialog", this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    auto *const info = new AboutInfo(dialog);
    dialog->setContextProperty("aboutInfo", info);
    dialog->setQmlSource(QUrl(QStringLiteral("qrc:/qt/qml/MMapper/AboutDialog.qml")));
    dialog->open();
#else
    auto *about = new AboutDialog(this);
    about->setAttribute(Qt::WA_DeleteOnClose);
    about->open();
#endif
}

void MainWindow::setMapModified(bool modified)
{
    setWindowModified(modified);
    saveAct->setEnabled(modified);
    if (modified) {
        deref(m_mapWindow).hideSplashImage();
    }
}

void MainWindow::updateMapModified()
{
    setMapModified(m_mapData->isModified());
    getCanvas()->update();
}

void MainWindow::showWarning(const QString &s)
{
    // REVISIT: shouldn't the warning have "this" as parent?
    // REVISIT: shouldn't this also say MMapper?
    QMessageBox::warning(nullptr, tr("Application"), s);
}

void MainWindow::showAsyncFailure(const QString &fileName,
                                  const AsyncIOTypeEnum mode,
                                  const bool wasCanceled)
{
    const char *const modeName = get_type_name(mode);
    const char *const msg = wasCanceled ? "User canceled the %1 of file %2"
                                        : "Failed to %1 file %2";
    showWarning(tr(msg).arg(modeName, fileName));
}

void MainWindow::slot_onFindRoom()
{
#ifdef MMAPPER_WITH_QML
    m_findRoomsDlg->restoreGeometry(getConfig().findRoomsDialog.geometry);
    m_findRoomsDlg->show();
    m_findRoomsDlg->raise();
    m_findRoomsDlg->activateWindow();
#else
    m_findRoomsDlg->show();
#endif
}

void MainWindow::slot_onLaunchClient()
{
    m_dockDialogClient->show();
}

void MainWindow::setCurrentFile(const QString &fileName)
{
    QFileInfo file(fileName);
    const auto shownName = fileName.isEmpty() ? "Untitled" : file.fileName();
    const auto fileSuffix = ((m_mapData && m_mapData->isFileReadOnly())
                             || (!fileName.isEmpty() && !file.isWritable()))
                                ? " [read-only]"
                                : "";
    const auto appSuffix = isMMapperBeta() ? " Beta" : "";

    // [*] is Qt black magic for only showing the '*' if the document has been modified.
    // From https://doc.qt.io/qt-5/qwidget.html:
    // > The window title must contain a "[*]" placeholder, which indicates where the '*' should appear.
    // > Normally, it should appear right after the file name (e.g., "document1.txt[*] - Text Editor").
    // > If the window isn't modified, the placeholder is simply removed.

    mmqt::setWindowTitle2(*this,
                          QString("MMapper%1").arg(appSuffix),
                          QString("%1[*]%2").arg(shownName, fileSuffix));
}

void MainWindow::slot_onLayerUp()
{
    getCanvas()->slot_layerUp();
}

void MainWindow::slot_onLayerDown()
{
    getCanvas()->slot_layerDown();
}

void MainWindow::slot_onLayerReset()
{
    getCanvas()->slot_layerReset();
}

void MainWindow::slot_onModeConnectionSelect()
{
    setCanvasMouseMode(CanvasMouseModeEnum::SELECT_CONNECTIONS);
}

void MainWindow::slot_onModeRoomRaypick()
{
    setCanvasMouseMode(CanvasMouseModeEnum::RAYPICK_ROOMS);
}

void MainWindow::slot_onModeRoomSelect()
{
    setCanvasMouseMode(CanvasMouseModeEnum::SELECT_ROOMS);
}

void MainWindow::slot_onModeMoveSelect()
{
    setCanvasMouseMode(CanvasMouseModeEnum::MOVE);
}

void MainWindow::slot_onModeCreateRoomSelect()
{
    setCanvasMouseMode(CanvasMouseModeEnum::CREATE_ROOMS);
}

void MainWindow::slot_onModeCreateConnectionSelect()
{
    setCanvasMouseMode(CanvasMouseModeEnum::CREATE_CONNECTIONS);
}

void MainWindow::slot_onModeCreateOnewayConnectionSelect()
{
    setCanvasMouseMode(CanvasMouseModeEnum::CREATE_ONEWAY_CONNECTIONS);
}

void MainWindow::slot_onModeInfomarkSelect()
{
    setCanvasMouseMode(CanvasMouseModeEnum::SELECT_INFOMARKS);
}

void MainWindow::slot_onModeCreateInfomarkSelect()
{
    setCanvasMouseMode(CanvasMouseModeEnum::CREATE_INFOMARKS);
}

void MainWindow::slot_onEditInfomarkSelection()
{
    if (m_infoMarkSelection == nullptr) {
        return;
    }

#ifdef MMAPPER_WITH_QML
    auto *const controller = new InfomarkEditController(this);
    auto *const dlg = new QmlDialog(tr("Edit Markers"), "InfomarksEditDlg", this);
    controller->setParent(dlg);
    controller->setInfomarkSelection(m_infoMarkSelection, m_mapData);
    dlg->setContextProperty("infomarkEditController", controller);
    dlg->setQmlSource(QUrl(QStringLiteral("qrc:/qt/qml/MMapper/InfomarkEditDialog.qml")));
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->restoreGeometry(getConfig().infomarksDialog.geometry);
    connect(dlg, &QDialog::finished, this, [dlg](int /*result*/) {
        setConfig().infomarksDialog.geometry = dlg->saveGeometry();
    });
    dlg->show();
#else
    auto *dlg = new InfomarksEditDlg(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setInfomarkSelection(m_infoMarkSelection, m_mapData, getCanvas());
    dlg->show();
#endif
}

void MainWindow::slot_onCreateRoom()
{
    deref(getCanvas()).slot_createRoom();
}

#ifdef MMAPPER_WITH_QML
namespace { // anonymous
// Ports roomeditattrdlg.cpp's closeEvent()/QDialog::finished handler pair:
// the [X]/Alt+F4 path (closeEvent()) is flatly refused while the note has
// unsaved edits (mirrors the widget's closeButton being disabled the same
// way, via RoomEditDialog.qml's "enabled: !roomEditController.noteDirty");
// the Escape path (QDialog::reject(), which bypasses closeEvent() entirely)
// is allowed through, but pops the widget's "ignored note" warning box
// first, quoting the about-to-be-discarded text.
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
            box->setWindowTitle(tr("[mmapper] warning: ignored note"));
            box->setText(m_controller->getNoteText());
            box->open();
        }
        QmlDialog::reject();
    }
};
} // namespace
#endif

#ifdef MMAPPER_WITH_QML
void MainWindow::slot_onEditRoomSelection()
{
    if (m_roomSelection == nullptr) {
        return;
    }

    if (m_roomEditDialog != nullptr) {
        m_roomEditDialog->setFocus();
        m_roomEditDialog->raise();
        m_roomEditDialog->activateWindow();
        return;
    }

    auto *const controller = new RoomEditController(nullptr);
    auto dialog = std::make_unique<RoomEditQmlDialog>(controller,
                                                      tr("Room properties"),
                                                      "RoomEditAttrDlg",
                                                      this);
    controller->setParent(dialog.get());
    controller->setRoomSelection(m_roomSelection, m_mapData);
    connect(controller,
            &RoomEditController::sig_requestUpdate,
            getCanvas(),
            &MapCanvas::slot_requestUpdate);
    connect(controller, &RoomEditController::sig_error, this, [this](const QString &message) {
        statusBar()->showMessage(message, 5000);
    });

    dialog->setContextProperty("roomEditController", controller);
    dialog->setQmlSource(QUrl(QStringLiteral("qrc:/qt/qml/MMapper/RoomEditDialog.qml")));
    dialog->restoreGeometry(getConfig().roomEditDialog.geometry);

    m_roomEditController = controller;
    m_roomEditDialog = std::move(dialog);

    auto &roomEditDialog = deref(m_roomEditDialog);
    roomEditDialog.show();
    connect(&roomEditDialog, &QDialog::finished, this, [this](MAYBE_UNUSED int result) {
        setConfig().roomEditDialog.geometry = m_roomEditDialog->saveGeometry();
        m_roomEditController = nullptr;
        m_roomEditDialog.reset();
    });
}
#else
void MainWindow::slot_onEditRoomSelection()
{
    if (m_roomSelection == nullptr) {
        return;
    }

    if (m_roomEditAttrDlg != nullptr) {
        m_roomEditAttrDlg->setFocus();
        return;
    }

    m_roomEditAttrDlg = std::make_unique<RoomEditAttrDlg>(this);
    {
        auto &roomEditDialog = deref(m_roomEditAttrDlg);
        roomEditDialog.setRoomSelection(m_roomSelection, m_mapData, getCanvas());
        roomEditDialog.show();
        connect(&roomEditDialog, &QDialog::finished, this, [this](MAYBE_UNUSED int result) {
            m_roomEditAttrDlg.reset();
        });
    }
}
#endif

void MainWindow::slot_onDeleteInfomarkSelection()
{
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

    MapCanvas *const canvas = getCanvas();
    canvas->slot_clearInfomarkSelection();
}

void MainWindow::slot_onDeleteRoomSelection()
{
    if (m_roomSelection == nullptr) {
        return;
    }

    applyGroupAction([](const RawRoom &room) -> Change {
        return Change{room_change_types::RemoveRoom{room.getId()}};
    });
    getCanvas()->slot_clearRoomSelection();
}

void MainWindow::slot_onDeleteConnectionSelection()
{
    if (m_connectionSelection == nullptr) {
        return; // previously called mapChanged for no good reason
    }

    const auto &first = m_connectionSelection->getFirst();
    const auto &second = m_connectionSelection->getSecond();
    const auto &r1 = first.room;
    const auto &r2 = second.room;
    if (!r1 || !r2) {
        return; // previously called mapChanged for no good reason
    }

    const ExitDirEnum dir1 = first.direction;
    MAYBE_UNUSED const ExitDirEnum dir2 = second.direction;
    const RoomId &id1 = r1.getId();
    const RoomId &id2 = r2.getId();

    getCanvas()->slot_clearConnectionSelection();

    // REVISIT: dir2?
    m_mapData->applySingleChange(Change{exit_change_types::ModifyExitConnection{
        ChangeTypeEnum::Remove, id1, dir1, id2, WaysEnum::TwoWay}});
}

bool MainWindow::slot_moveRoomSelection(const Coordinate offset)
{
    if (m_roomSelection == nullptr) {
        return false;
    }

    applyGroupAction([&offset](const RawRoom &room) -> Change {
        return Change{room_change_types::MoveRelative{room.getId(), offset}};
    });
    return true;
}

void MainWindow::slot_onMoveUpRoomSelection()
{
    if (!slot_moveRoomSelection(Coordinate(0, 0, 1))) {
        return;
    }

    slot_onLayerUp();
}

void MainWindow::slot_onMoveDownRoomSelection()
{
    if (!slot_moveRoomSelection(Coordinate(0, 0, -1))) {
        return;
    }

    slot_onLayerDown();
}

bool MainWindow::slot_mergeRoomSelection(const Coordinate offset)
{
    if (m_roomSelection == nullptr) {
        return false;
    }

    applyGroupAction([&offset](const RawRoom &room) -> Change {
        return Change{room_change_types::MergeRelative{room.getId(), offset}};
    });
    return true;
}

void MainWindow::slot_onMergeUpRoomSelection()
{
    if (!slot_mergeRoomSelection(Coordinate(0, 0, 1))) {
        return;
    }

    slot_onLayerUp();
    slot_onModeRoomSelect();
}

void MainWindow::slot_onMergeDownRoomSelection()
{
    if (!slot_mergeRoomSelection(Coordinate(0, 0, -1))) {
        return;
    }

    slot_onLayerDown();
    slot_onModeRoomSelect();
}

void MainWindow::slot_onConnectToNeighboursRoomSelection()
{
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
}

void MainWindow::slot_forceMapperToRoom()
{
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
}

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
#endif
void MainWindow::slot_onCheckForUpdate()
{
    assert(!NO_UPDATER);
    m_updateDialog->show();
#ifdef MMAPPER_WITH_QML
    m_updateDialog->raise();
    m_updateChecker->check();
#else
    m_updateDialog->open();
#endif
}
#ifdef __clang__
#pragma clang diagnostic pop
#endif

void MainWindow::slot_voteForMUME()
{
    QDesktopServices::openUrl(QUrl(
        "https://www.mudconnect.com/cgi-bin/search.cgi?mode=mud_listing&mud=MUME+-+Multi+Users+In+Middle+Earth"));
}

void MainWindow::slot_openMumeWebsite()
{
    QDesktopServices::openUrl(QUrl("https://mume.org/"));
}

void MainWindow::slot_openMumeForum()
{
    QDesktopServices::openUrl(QUrl("https://mume.org/forum/"));
}

void MainWindow::slot_openMumeWiki()
{
    QDesktopServices::openUrl(QUrl("https://mume.org/wiki/"));
}

void MainWindow::slot_openSettingUpMmapper()
{
    QDesktopServices::openUrl(QUrl("https://github.com/MUME/MMapper/wiki/Troubleshooting"));
}

void MainWindow::onReportIssueTriggered()
{
    QDesktopServices::openUrl(QUrl("https://github.com/MUME/MMapper/issues"));
}

void MainWindow::slot_openNewbieHelp()
{
    QDesktopServices::openUrl(QUrl("https://mume.org/newbie.php"));
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        m_mapWindow->keyPressEvent(event);
        return;
    }
    QWidget::keyPressEvent(event);
}

void MainWindow::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        m_mapWindow->keyReleaseEvent(event);
        return;
    }
    QWidget::keyReleaseEvent(event);
}

MapCanvas *MainWindow::getCanvas() const
{
    return m_mapWindow->getCanvas();
}

void MainWindow::mapChanged() const
{
    if (MapCanvas *const canvas = getCanvas()) {
        canvas->slot_mapChanged();
    }
}

void MainWindow::setCanvasMouseMode(const CanvasMouseModeEnum mode)
{
    if (MapCanvas *const canvas = getCanvas()) {
        canvas->slot_setCanvasMouseMode(mode);
    }
}

void MainWindow::applyGroupAction(const std::function<Change(const RawRoom &)> &getChange)
{
    m_mapData->applyChangesToList(deref(m_roomSelection), getChange);
}

void MainWindow::showStatusInternal(const QString &text, int duration)
{
    statusBar()->showMessage(text, duration);
}

void MainWindow::onSuccessfulLoad(const MapLoadData &mapLoadData)
{
    auto &mapData = deref(m_mapData);
    auto &mapCanvas = deref(getCanvas());
#ifdef MMAPPER_WITH_QML
    auto &groupWidget = deref(m_groupController);
#else
    auto &groupWidget = deref(m_groupWidget);
#endif
    auto &pathMachine = deref(m_pathMachine);

    mapData.setMapData(mapLoadData);
    mapData.checkSize();

    // TODO: convert from slot_xxx prefix if these are no longer used as slots anywhere.
    mapCanvas.slot_dataLoaded();
    groupWidget.slot_mapLoaded();
    pathMachine.onMapLoaded();
    if (const auto room = mapData.getCurrentRoom()) {
        updateDescriptionRoom(room);
        deref(m_audioManager).onAreaChanged(room.getArea());
    }

    // Should this be part of mapChanged?
    updateMapModified();
    mapChanged();

    setCurrentFile(mapData.getFileName());
    showStatusShort(tr("File loaded"));
}

void MainWindow::onSuccessfulMerge(const Map &map)
{
    auto &mapData = deref(m_mapData);
    auto &mapCanvas = deref(getCanvas());
#ifdef MMAPPER_WITH_QML
    auto &groupWidget = deref(m_groupController);
#else
    auto &groupWidget = deref(m_groupWidget);
#endif

    mapData.setCurrentMap(map);
    mapData.checkSize();

    // FIXME: mapData.setMapData() or mapData.checkSize() kicks off an async remesh,
    // and then slot_dataLoaded() immediately sets it to be ignored and starts another
    // one in parallel? (see: ignorePendingRemesh)

    mapCanvas.slot_dataLoaded();
    groupWidget.slot_mapLoaded();
    updateMapModified();
    mapChanged();
    showStatusShort(tr("File merged"));
}

void MainWindow::onSuccessfulSave(const SaveModeEnum mode,
                                  const SaveFormatEnum format,
                                  const QString &fileName)
{
    auto &mapData = deref(m_mapData);

    if (mode == SaveModeEnum::FULL && format == SaveFormatEnum::MM2) {
        mapData.setFileName(fileName, !QFileInfo(fileName).isWritable());
        setCurrentFile(fileName);
        mapData.currentHasBeenSaved();
    }

    showStatusShort(tr("File saved"));
    updateMapModified();

    QFileInfo file{fileName};

    // Update last directory
    auto &config = setConfig().autoLoad;
    config.lastMapDirectory = file.absoluteDir().absolutePath();

    if constexpr (CURRENT_PLATFORM == PlatformEnum::Wasm) {
        return;
    }

    const QString &absoluteFilePath = file.absoluteFilePath();
    if (!config.autoLoadMap || config.fileName != absoluteFilePath) {
        // Check if this should be the new autoload map
        auto *dlg = new QMessageBox(QMessageBox::Question,
                                    "Autoload Map?",
                                    "Autoload this map when MMapper starts?",
                                    QMessageBox::StandardButtons{QMessageBox::Yes | QMessageBox::No},
                                    this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        connect(dlg, &QMessageBox::finished, this, [&config, absoluteFilePath](int result) {
            if (result == QMessageBox::Yes) {
                config.autoLoadMap = true;
                config.fileName = absoluteFilePath;
            }
        });
        dlg->open();
    }
}
