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

void RoomSignalHandler::hold(const RoomId room, PathProcessor *const locker) // Changed to PathProcessor*
{
    // REVISIT: why do we allow locker to be null?
    owners.insert(room);
    if (lockers[room].empty()) {
        holdCount[room] = 0;
    }
    lockers[room].insert(locker);
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
            assert(false);
        }

        lockers.erase(room);
        owners.erase(room);
    }
}

void RoomSignalHandler::keep(const RoomId room,
                             const ExitDirEnum dir,
                             const RoomId fromId,
                             ChangeList &changes)
{
    assert(holdCount[room] != 0);
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
    if (!lockers[room].empty()) {
        if (PathProcessor *const locker = *(lockers[room].begin())) { // Changed to PathProcessor*
            // m_map.keepRoom(*locker, room); // Removed
            lockers[room].erase(locker);
        } else {
            assert(false);
        }
    }
    release(room, changes); // This will decrement holdCount and might trigger actual release logic
}
