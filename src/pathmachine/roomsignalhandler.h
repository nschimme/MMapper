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
#include <set>    // For RoomIdSet (already used by m_owners) and for m_lockers' value type

#include <QObject>
#include <QString>
#include <QtCore>

// class RoomRecipient; // Replaced by PathProcessor
#include "PathProcessor.h" // Include PathProcessor since it's in the same directory
struct RoomId;
class MapFrontend;

/**
 * @brief Manages room lifecycle signals and "holds" during pathfinding.
 *
 * RoomSignalHandler is responsible for tracking which PathProcessor strategies
 * or Path objects have an active interest in a particular RoomId. This is done
 * primarily through a "hold count" (`m_holdCount`) and a collection of unique weak "locker"
 * references (`m_lockers`, storing `std::weak_ptr<PathProcessor>`).
 *
 * Key functionalities:
 * - `hold(RoomId, std::weak_ptr<PathProcessor>)`: Called by a "locker" to indicate
 *   it's currently using or evaluating a room. Increments hold count and stores
 *   a weak reference.
 * - `release(RoomId, ChangeList&)`: Decrements hold count. If zero for a temporary
 *   room, queues its removal. Clears room entries.
 * - `keep(RoomId, ExitDirEnum, RoomId, ChangeList&)`: Converts a "hold" to a
 *   "kept" state. Makes room permanent if temporary, may add an exit, adjusts
 *   locker tracking, then calls `release()`.
 * - `getNumLockers(RoomId)`: Provides a count of registered locker entries for a
 *   room. (Note: Current simple version counts all registered weak_ptrs, not
 *   just non-expired ones, for behavioral consistency with past heuristics).
 *
 * Owned by PathMachine, it queues changes to a ChangeList rather than applying
 * them directly.
 */
class NODISCARD_QOBJECT RoomSignalHandler final : public QObject
{
    Q_OBJECT

private:
    MapFrontend &m_map;
    RoomIdSet m_owners; // Prefixed
    std::map<RoomId, std::set<PathProcessor*>> m_lockers; // Changed to PathProcessor*
    std::map<RoomId, int> m_holdCount; // Prefixed

public:
    RoomSignalHandler() = delete;
    explicit RoomSignalHandler(MapFrontend &map, QObject *parent)
        : QObject(parent)
        , m_map{map}
    {}
    /* receiving from our clients: */
    // hold the room, we don't know yet what to do, overrides release, re-caches if room was un-cached
    void hold(RoomId room, PathProcessor* locker); // Changed to PathProcessor*
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
