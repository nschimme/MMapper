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

// PathProcessor.h and WeakHandle.h are expected to be included via roomsignalhandler.h

void RoomSignalHandler::hold(const RoomId room, WeakHandle<PathProcessor> locker_handle)
{
    owners.insert(room);
    // Initialize holdCount for the room if it's not already tracked.
    if (holdCount.find(room) == holdCount.end()) {
        holdCount[room] = 0;
    }
    // 'lockers' is now std::map<RoomId, std::vector<WeakHandle<PathProcessor>>>
    lockers[room].push_back(locker_handle);
    ++holdCount[room];
}

void RoomSignalHandler::release(const RoomId room, ChangeList &changes)
{
    assert(holdCount[room]);
    if (--holdCount[room] == 0) {
        if (owners.contains(room)) {
            // New logic to remove room if it's temporary
            if (auto rh = m_map.findRoomHandle(room)) {
                if (rh.isTemporary()) {
                    changes.add(Change{room_change_types::RemoveRoom{room}});
                }
            }
            // Original loop that called m_map.releaseRoom for each recipient is removed.
        } else {
            // This case might indicate a logic error if a room can be released without being an owner
            // For now, maintaining original assertion if present, or consider logging
            assert(false);
        }
        lockers.erase(room); // Clear the vector for the room
        owners.erase(room);
        holdCount.erase(room); // Also clean up holdCount
    }
}

void RoomSignalHandler::keep(const RoomId room,
                             const ExitDirEnum dir,
                             const RoomId fromId,
                             ChangeList &changes)
{
    // Corrected assertion to avoid inserting into map with operator[] if key doesn't exist
    assert(holdCount.count(room) && holdCount.at(room) != 0);
    assert(owners.contains(room));

    // New logic to make room permanent if it's temporary
    if (auto rh = m_map.findRoomHandle(room)) {
        if (rh.isTemporary()) {
            changes.add(Change{room_change_types::MakePermanent{room}});
        }
    }

    static_assert(static_cast<uint32_t>(ExitDirEnum::UNKNOWN) + 1 == NUM_EXITS);
    if (isNESWUD(dir) || dir == ExitDirEnum::UNKNOWN) {
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
    // If 'lockers' contains the room and its vector of handles is not empty,
    // remove one element to simulate the old behavior of reducing the locker count.
    auto it_room_lockers = lockers.find(room);
    if (it_room_lockers != lockers.end()) {
        auto& handles_vector = it_room_lockers->second;
        if (!handles_vector.empty()) {
            handles_vector.pop_back(); // Simple way to remove one element
        }
    }
    // === End of new logic ===

    release(room, changes); // This will decrement holdCount and might trigger actual release logic
}

int RoomSignalHandler::getNumLockers(RoomId room) {
    auto it = lockers.find(room);
    if (it == lockers.end()) {
        return 0;
    }
    // TODO: This count includes expired WeakHandles and is not an accurate
    // representation of live lockers. It's preserved to maintain existing
    // pathfinding heuristic behavior, which might be sensitive to changes
    // in this count. Consider refining this or the heuristic in the future,
    // or implementing a separate cleanup mechanism for the 'lockers' vector.
    return static_cast<int>(it->second.size());
}
