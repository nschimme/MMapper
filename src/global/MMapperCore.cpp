// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "MMapperCore.h"

#include "../configuration/configuration.h"

MMapperCore::MMapperCore(QObject *parent)
    : QObject(parent)
{
    m_mapData = new MapData(this);
    m_prespammedPath = new PrespammedPath(this);
    m_groupManager = new Mmapper2Group(this);
    m_pathMachine = new Mmapper2PathMachine(*m_mapData, this);
    m_gameObserver = std::make_unique<GameObserver>();
    m_adventureTracker = new AdventureTracker(*m_gameObserver, this);
    m_roomManager = new RoomManager(this);

    m_gameObserver->sig2_sentToUserGmcp.connect(m_lifetime, [this](const GmcpMessage &gmcp) {
        m_roomManager->slot_parseGmcpInput(gmcp);
    });

    m_mumeClock = new MumeClock(getConfig().mumeClock.startEpoch, *m_gameObserver, this);
    m_logger = new AutoLogger(this);

    GameObserver &observer = *m_gameObserver;
    observer.sig2_connected.connect(m_lifetime, [this]() { m_logger->slot_onConnected(); });
    observer.sig2_toggledEchoMode.connect(m_lifetime,
                                          [this](bool echo) { m_logger->slot_shouldLog(echo); });
    observer.sig2_sentToMudString.connect(m_lifetime, [this](const QString &msg) {
        m_logger->slot_writeToLog(msg);
    });
    observer.sig2_sentToUserString.connect(m_lifetime, [this](const QString &msg) {
        m_logger->slot_writeToLog(msg);
    });

    m_hotkeyManager = std::make_unique<HotkeyManager>();
    m_remoteEdit = new RemoteEdit(this);

    connect(m_mapData, &MapData::sig_log, this, &MMapperCore::sig_log);
    connect(m_groupManager, &Mmapper2Group::sig_log, this, &MMapperCore::sig_log);
    connect(m_mumeClock, &MumeClock::sig_log, this, &MMapperCore::sig_log);
}

void MMapperCore::setMapCanvas(MapCanvas *canvas)
{
    if (m_listener) {
        delete m_listener;
    }
    m_listener = new ConnectionListener(*this, *canvas, this);
    connect(m_listener, &ConnectionListener::sig_log, this, &MMapperCore::sig_log);
}

void MMapperCore::startServices()
{
    if (!m_listener)
        return;
    try {
        m_listener->listen();
        emit sig_log("ConnectionListener",
                     tr("Server bound on localhost to port: %1.")
                         .arg(getConfig().connection.localPort));
    } catch (const std::exception &e) {
        emit sig_log("ConnectionListener", tr("Unable to start the server: %1.").arg(e.what()));
    }
}

MMapperCore::~MMapperCore() = default;
