// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "roomsignalhandler.h"

#include "../map/ChangeTypes.h" // Required for room_change_types
#include "../map/Map.h" // Required for RoomHandle::isTemporary, via m_map
#include "../mapdata/mapdata.h" // Required for MapFrontend

#include <cassert>

void RoomSignalHandler::hold(const RoomId room)
{
    m_owners.insert(room);
    m_holdCount[room]++;
}

void RoomSignalHandler::release(const RoomId room, ChangeList &masterChanges)
{
    auto it_hold_count = m_holdCount.find(room);
    assert(it_hold_count != m_holdCount.end() && "Room not found in holdCount during release");
    assert(it_hold_count->second > 0 && "Hold count is already zero or less during release");

    if (--it_hold_count->second == 0) {
        if (m_owners.contains(room)) {
            // Check if the room is temporary before attempting to remove
            if (RoomHandle rh = m_map.findRoomHandle(room)) {
                if (rh.isTemporary()) {
                    masterChanges.add(room_change_types::RemoveRoom{room});
                }
            }
        } else {
            // This assertion indicates a logic error elsewhere if a room is released
            // without being properly tracked as "owned" by the signal handler.
            assert(false && "Room released was not in m_owners");
        }

        m_owners.erase(room);
        m_holdCount.erase(it_hold_count); // Use iterator to erase from map
    }
}

void RoomSignalHandler::keep(const RoomId room,
                             const ExitDirEnum dir,
                             const RoomId fromId,
                             ChangeList &pathChanges,
                             ChangeList &masterChanges)
{
    auto it_hold_count = m_holdCount.find(room);
    assert(it_hold_count != m_holdCount.end() && "Room not found in holdCount during keep");
    assert(it_hold_count->second != 0 && "Hold count is zero during keep");
    assert(m_owners.contains(room) && "Room kept was not in m_owners");

    static_assert(static_cast<uint32_t>(ExitDirEnum::UNKNOWN) + 1 == NUM_EXITS);
    if (isNESWUD(dir) || dir == ExitDirEnum::UNKNOWN) {
        pathChanges.add(exit_change_types::ModifyExitConnection{ChangeTypeEnum::Add,
                                                              fromId,
                                                              dir,
                                                              room,
                                                              WaysEnum::OneWay});
    }

    // If the room is temporary, add a change to make it permanent
    if (RoomHandle rh = m_map.findRoomHandle(room)) {
        if (rh.isTemporary()) {
            masterChanges.add(room_change_types::MakePermanent{room});
        }
    }

    // Release the hold, as 'keep' implies it's now accounted for (e.g., permanent or part of an approved path)
    release(room, masterChanges);
}

size_t RoomSignalHandler::getNumHolders(const RoomId room) const
{
    auto it = m_holdCount.find(room);
    if (it != m_holdCount.end()) {
        return it->second;
    }
    return 0;
}
