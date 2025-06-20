// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "roomsignalhandler.h"

#include "../map/AbstractChangeVisitor.h"
#include "../map/room.h"
#include "../map/roomid.h"
#include "../mapdata/mapdata.h"

#include <cassert>
#include <memory>
#include <algorithm> // Required for std::remove_if
#include <memory>    // For std::owner_less if it were used with smart pointers, not PathProcessor* directly

// PathProcessor.h is included via roomsignalhandler.h

void RoomSignalHandler::hold(const RoomId room, PathProcessor* locker) // Changed to PathProcessor*
{
    m_owners.insert(room);
    // Initialize m_holdCount for the room if it's not already tracked.
    if (m_holdCount.find(room) == m_holdCount.end()) {
        m_holdCount[room] = 0;
    }
    // 'm_lockers' is std::map<RoomId, std::set<PathProcessor*>>
    m_lockers[room].insert(locker); // Storing PathProcessor*
    ++m_holdCount[room];
}

ReleaseDecision RoomSignalHandler::release(const RoomHandle &roomHandle) // Changed signature
{
    const RoomId roomId = roomHandle.getId();
    ReleaseDecision decision;
    decision.roomId = roomId;

    assert(m_holdCount.count(roomId) && m_holdCount.at(roomId) > 0);
    if (--m_holdCount[roomId] == 0) {
        if (m_owners.contains(roomId)) {
            // Room is temporary, no longer held by any locker, and not pending permanent in this cycle.
            if (roomHandle.isTemporary() && m_pendingPermanentThisCycle.find(roomId) == m_pendingPermanentThisCycle.end()) {
                decision.shouldRemoveRoom = true;
                m_pendingRemovalThisCycle.insert(roomId);
            }
        } else {
            // This case might indicate a logic error if a room can be released without being an owner
            assert(false);
        }
        m_lockers.erase(roomId);
        m_owners.erase(roomId);
        m_holdCount.erase(roomId);
    }
    return decision;
}

KeepDecision RoomSignalHandler::keep(const RoomHandle &roomHandle, // Changed signature
                                     const ExitDirEnum dir,
                                     const RoomId fromId)
{
    const RoomId roomId = roomHandle.getId();
    KeepDecision decision;
    decision.roomId = roomId;

    assert(m_holdCount.count(roomId) && m_holdCount.at(roomId) != 0);
    assert(m_owners.contains(roomId));

    // Decide if room should be made permanent
    if (roomHandle.isTemporary() && m_pendingRemovalThisCycle.find(roomId) == m_pendingRemovalThisCycle.end()) {
        decision.shouldMakePermanent = true;
        m_pendingPermanentThisCycle.insert(roomId);
    }

    // Decide if an exit should be modified
    static_assert(static_cast<uint32_t>(ExitDirEnum::UNKNOWN) + 1 == NUM_EXITS);
    if (isNESWUD(dir) || dir == ExitDirEnum::UNKNOWN) {
        decision.exitToModify = exit_change_types::ModifyExitConnection{
            ChangeTypeEnum::Add,
            fromId,
            dir,
            roomId,
            WaysEnum::OneWay
        };
    }

    // Existing logic to remove one locker entry
    auto it_room_lockers = m_lockers.find(roomId);
    if (it_room_lockers != m_lockers.end()) {
        auto& handles_set = it_room_lockers->second;
        if (!handles_set.empty()) {
            handles_set.erase(handles_set.begin());
        }
    }

    this->release(roomHandle); // Call new release, ignore its decision for keep's decision

    return decision;
}

int RoomSignalHandler::getNumLockers(RoomId room) {
    auto it = m_lockers.find(room);
    if (it == m_lockers.end()) {
        return 0;
    }
    // TODO: This count includes expired WeakHandles and is not an accurate
    // representation of live lockers. It's preserved to maintain existing
    // pathfinding heuristic behavior, which might be sensitive to changes
    // in this count. Consider refining this or the heuristic in the future,
    // or implementing a separate cleanup mechanism for the 'lockers' vector.
    return static_cast<int>(it->second.size());
}

void RoomSignalHandler::clearPendingStatesForCycle() {
    m_pendingPermanentThisCycle.clear();
    m_pendingRemovalThisCycle.clear();
}
