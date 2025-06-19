#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "../map/ChangeList.h"
#include "../map/DoorFlags.h"
#include "../map/ExitDirection.h"
#include "../map/ExitFieldVariant.h"
#include "../map/ExitFlags.h"
#include "../map/roomid.h"

#include <map>
#include <memory>
#include <set>

#include <QObject>
#include <QString>
#include <QtCore>

// class RoomRecipient; // Replaced by PathProcessor
#include "PathProcessor.h" // Include PathProcessor since it's in the same directory
#include "../global/WeakHandle.h" // Added for WeakHandle
struct RoomId;
class MapFrontend;

class NODISCARD_QOBJECT RoomSignalHandler final : public QObject
{
    Q_OBJECT

private:
    MapFrontend &m_map;
    RoomIdSet owners;
    std::map<RoomId, std::vector<WeakHandle<PathProcessor>>> lockers; // Changed to vector of WeakHandles
    std::map<RoomId, int> holdCount;

public:
    RoomSignalHandler() = delete;
    explicit RoomSignalHandler(MapFrontend &map, QObject *parent)
        : QObject(parent)
        , m_map{map}
    {}
    /* receiving from our clients: */
    // hold the room, we don't know yet what to do, overrides release, re-caches if room was un-cached
    void hold(RoomId room, WeakHandle<PathProcessor> locker_handle); // Changed to WeakHandle
    // room isn't needed anymore and can be deleted if no one else is holding it and no one else uncached it
    void release(RoomId room, ChangeList &changes);
    // keep the room but un-cache it - overrides both hold and release
    // toId is negative if no exit should be added, else it's the id of
    // the room where the exit should lead
    void keep(RoomId room, ExitDirEnum dir, RoomId fromId, ChangeList &changes);

    /* Sending to the rooms' owners:
       keepRoom: keep the room, but we don't need it anymore for now
       releaseRoom: delete the room, if you like */

    int getNumLockers(RoomId room); // Moved to .cpp

signals:
    void sig_scheduleAction(const SigMapChangeList &);
};
