// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "roomsignalhandler.h"
#include "patheventcontext.h" // Include for mmapper::PathEventContext

#include "../mapdata/mapdata.h"

#include <cassert>

void RoomSignalHandler::hold(const RoomId room)
{
    m_owners.insert(room);
    // operator[] will default-construct int to 0 if 'room' is not found, then increment.
    // This correctly handles both initialization and increment.
    m_holdCount[room]++;
}

// void RoomSignalHandler::release(const RoomId room)
void RoomSignalHandler::release(mmapper::PathEventContext &context, const RoomId room) // Added context
{
    auto it_hold_count = m_holdCount.find(room);
    assert(it_hold_count != m_holdCount.end() && it_hold_count->second > 0);

    if (--it_hold_count->second == 0) {
        if (m_owners.contains(room)) {
            // if (auto rh = m_map.findRoomHandle(room)) { becomes:
            if (auto rh = context.map.findRoomHandle(room)) { // Use context.map
                if (rh.isTemporary()) {
                    // Use addTrackedChange
                    context.addTrackedChange(Change{room_change_types::RemoveRoom{room}});
                }
            }
        } else {
            assert(false);
        }

        // m_holders.erase(room); // Removed
        m_owners.erase(room);
        m_holdCount.erase(room); // or it_hold_count, but room is fine as key
    }
}

// void RoomSignalHandler::keep(const RoomId room, const ExitDirEnum dir, const RoomId fromId, ChangeList &changes)
void RoomSignalHandler::keep(mmapper::PathEventContext &context, // Changed ChangeList to context
                             const RoomId room,
                             const ExitDirEnum dir,
                             const RoomId fromId)
{
    auto it_hold_count = m_holdCount.find(room);
    assert(it_hold_count != m_holdCount.end() && it_hold_count->second != 0);
    assert(m_owners.contains(room));

    static_assert(static_cast<uint32_t>(ExitDirEnum::UNKNOWN) + 1 == NUM_EXITS);
    if (isNESWUD(dir) || dir == ExitDirEnum::UNKNOWN) {
        // changes.add becomes context.changes.add
        context.changes.add(exit_change_types::ModifyExitConnection{ChangeTypeEnum::Add,
                                                                    fromId,
                                                                    dir,
                                                                    room,
                                                                    WaysEnum::OneWay});
    }

    // Simplified logic for making room permanent
    // if (auto rh = m_map.findRoomHandle(room)) { becomes:
    if (auto rh = context.map.findRoomHandle(room)) { // Use context.map
        if (rh.isTemporary()) {
            // REVISIT: Use changes.add() instead of applySingleChange() and release()?
            // This comment is from the original code. The action itself is preserved.
            // Use addTrackedChange for MakePermanent
            context.addTrackedChange(Change{room_change_types::MakePermanent{room}});
        }
    }
    // release(room); becomes:
    release(context, room);
}
