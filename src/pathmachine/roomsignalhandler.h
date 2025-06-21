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
#include <set>

#include <QString>

class PathProcessor;
struct RoomId;
class MapFrontend;

/*!
 * @brief Manages room lifecycle signals and "holds" during pathfinding.
 *
 * RoomSignalHandler is responsible for tracking which PathProcessor strategies
 * or Path objects have an active interest in a particular RoomId. This is done
 * primarily through a "hold count" (`m_holdCount`) and a collection of "locker"
 * references (`m_lockers`, storing `PathProcessor*`).
 *
 * Key functionalities:
 * - `hold(RoomId, PathProcessor*)`: Called by a "locker" to indicate
 *   it's currently using or evaluating a room. Increments hold count and stores
 *   a raw pointer.
 * - `release(const RoomHandle&, ChangeList&)`: Decrements hold count. If zero for a temporary
 *   room, queues its removal to the ChangeList. Clears room entries.
 * - `keep(const RoomHandle&, ExitDirEnum, RoomId, ChangeList&)`: Converts a "hold" to a
 *   "kept" state. Directly adds changes (MakePermanent, ModifyExitConnection) to the
 *   provided ChangeList. Adjusts locker tracking, then calls `release()`.
 * - `getNumLockers(RoomId)`: Provides a count of registered locker entries for a
 *   room. (Note: Current simple version counts all registered raw pointers, not
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
    RoomIdSet m_owners;
    std::map<RoomId, int> m_holdCount;

public:
    RoomSignalHandler() = delete;
    explicit RoomSignalHandler(MapFrontend &map, QObject *parent)
        : QObject(parent)
        , m_map{map}
    {}
    /* receiving from our clients: */
    // hold the room, we don't know yet what to do, overrides release, re-caches if room was un-cached
    void hold(RoomId room);
    // room isn't needed anymore and can be deleted if no one else is holding it and no one else uncached it
    void release(RoomId room);
    // keep the room but un-cache it - overrides both hold and release
    // toId is negative if no exit should be added, else it's the id of
    // the room where the exit should lead
    void keep(RoomId room, ExitDirEnum dir, RoomId fromId, ChangeList &changes);

    /* Sending to the rooms' owners:
       keepRoom: keep the room, but we don't need it anymore for now
       releaseRoom: delete the room, if you like */

    auto getNumHolders(RoomId room) const -> int
    {
        auto it = m_holdCount.find(room);
        if (it != m_holdCount.end()) {
            return it->second;
        }
        return 0;
    }
};
