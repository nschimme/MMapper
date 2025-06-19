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

// PathProcessor.h is included via roomsignalhandler.h
// std::weak_ptr is from <memory> which is also included via roomsignalhandler.h

void RoomSignalHandler::hold(const RoomId room, std::weak_ptr<PathProcessor> locker_handle) // Changed to std::weak_ptr
{
    m_owners.insert(room);
    // Initialize m_holdCount for the room if it's not already tracked.
    if (m_holdCount.find(room) == m_holdCount.end()) {
        m_holdCount[room] = 0;
    }
    // 'm_lockers' is std::map<RoomId, std::set<std::weak_ptr<PathProcessor>, std::owner_less<...>>>
    m_lockers[room].insert(locker_handle); // Changed to insert for std::set
    ++m_holdCount[room];
}

void RoomSignalHandler::release(const RoomId room, ChangeList &changes)
{
    assert(m_holdCount.count(room) && m_holdCount.at(room) > 0); // Use .count or .at for safety, and m_ prefix
    if (--m_holdCount[room] == 0) {
        if (m_owners.contains(room)) {
            // New logic to remove room if it's temporary
            if (auto rh = m_map.findRoomHandle(room)) {
                if (rh.isTemporary()) {
                    // Room is temporary and no longer held by any locker; remove it.
                    changes.add(Change{room_change_types::RemoveRoom{room}});
                }
            }
            // Original loop that called m_map.releaseRoom for each recipient is removed.
        } else {
            // This case might indicate a logic error if a room can be released without being an owner
            // For now, maintaining original assertion if present, or consider logging
            assert(false);
        }
        m_lockers.erase(room); // Clear the vector for the room
        m_owners.erase(room);
        m_holdCount.erase(room); // Also clean up m_holdCount
    }
}

void RoomSignalHandler::keep(const RoomId room,
                             const ExitDirEnum dir,
                             const RoomId fromId,
                             ChangeList &changes)
{
    // Corrected assertion to avoid inserting into map with operator[] if key doesn't exist
    assert(m_holdCount.count(room) && m_holdCount.at(room) != 0);
    assert(m_owners.contains(room));

    // New logic to make room permanent if it's temporary
    if (auto rh = m_map.findRoomHandle(room)) {
        if (rh.isTemporary()) {
            // This room is being kept and was temporary; make it permanent.
            changes.add(Change{room_change_types::MakePermanent{room}});
        }
    }

    static_assert(static_cast<uint32_t>(ExitDirEnum::UNKNOWN) + 1 == NUM_EXITS);
    if (isNESWUD(dir) || dir == ExitDirEnum::UNKNOWN) {
        // If an exit direction is specified, add a connection from 'fromId' to this 'room'.
        changes.add(exit_change_types::ModifyExitConnection{ChangeTypeEnum::Add,
                                                            fromId,
                                                            dir,
                                                            room,
                                                            WaysEnum::OneWay});
    }

    // Original logic for managing lockers, but without calling m_map.keepRoom
    // The specific locker removal from the list is no longer needed here.
    // holdCount and release will manage the lifecycle.
    // The old code was iterating a std::set and erasing one PathProcessor*.
    // With std::vector<WeakHandle<PathProcessor>>, we don't do that here.
    // If a specific locker needs to be "released" from this room's perspective,
    // that would be a more complex operation (finding its WeakHandle and removing it),
    // but current logic seems to rely on holdCount and the general release.
    // The line `lockers[room].erase(locker);` from the previous state is removed.

    // === Add new logic here ===
    // If 'm_lockers' contains the room and its set of handles is not empty,
    // remove one element to simulate the old behavior of reducing the locker count.
    auto it_room_lockers = m_lockers.find(room);
    if (it_room_lockers != m_lockers.end()) {
        auto& handles_set = it_room_lockers->second; // Now a std::set
        if (!handles_set.empty()) {
            handles_set.erase(handles_set.begin()); // Remove an arbitrary element from the set
        }
    }
    // === End of new logic ===

    release(room, changes); // This will decrement holdCount and might trigger actual release logic
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
