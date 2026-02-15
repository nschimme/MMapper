#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "../adventure/adventuretracker.h"
#include "../client/HotkeyManager.h"
#include "../clock/mumeclock.h"
#include "../display/prespammedpath.h"
#include "../group/mmapper2group.h"
#include "../logger/autologger.h"
#include "../mapdata/mapdata.h"
#include "../mpi/remoteedit.h"
#include "../observer/gameobserver.h"
#include "../pathmachine/mmapper2pathmachine.h"
#include "../proxy/connectionlistener.h"
#include "../roompanel/RoomManager.h"

#include <memory>

#include <QObject>

class MMapperCore : public QObject
{
    Q_OBJECT
public:
    explicit MMapperCore(QObject *parent = nullptr);
    ~MMapperCore() override;

    void setMapCanvas(MapCanvas *canvas);
    void startServices();

    MapData &mapData() { return *m_mapData; }
    PrespammedPath &prespammedPath() { return *m_prespammedPath; }
    Mmapper2Group &groupManager() { return *m_groupManager; }
    Mmapper2PathMachine &pathMachine() { return *m_pathMachine; }
    GameObserver &gameObserver() { return *m_gameObserver; }
    AdventureTracker &adventureTracker() { return *m_adventureTracker; }
    RoomManager &roomManager() { return *m_roomManager; }
    MumeClock &mumeClock() { return *m_mumeClock; }
    AutoLogger &logger() { return *m_logger; }
    ConnectionListener *listener() { return m_listener; }
    HotkeyManager &hotkeyManager() { return *m_hotkeyManager; }
    RemoteEdit &remoteEdit() { return *m_remoteEdit; }

signals:
    void sig_log(const QString &module, const QString &message);

private:
    MapData *m_mapData = nullptr;
    PrespammedPath *m_prespammedPath = nullptr;
    Mmapper2Group *m_groupManager = nullptr;
    Mmapper2PathMachine *m_pathMachine = nullptr;
    std::unique_ptr<GameObserver> m_gameObserver;
    AdventureTracker *m_adventureTracker = nullptr;
    RoomManager *m_roomManager = nullptr;
    MumeClock *m_mumeClock = nullptr;
    AutoLogger *m_logger = nullptr;
    ConnectionListener *m_listener = nullptr;
    std::unique_ptr<HotkeyManager> m_hotkeyManager;
    RemoteEdit *m_remoteEdit = nullptr;

    Signal2Lifetime m_lifetime;
};
