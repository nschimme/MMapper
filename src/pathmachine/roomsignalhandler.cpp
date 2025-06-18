// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "roomsignalhandler.h"

#include "../map/AbstractChangeVisitor.h"
// RoomRecipient.h will be removed if not needed otherwise
#include "../map/room.h"
#include "../map/roomid.h"
#include "../mapdata/mapdata.h"

#include <cassert>
#include <memory>

void RoomSignalHandler::hold(const RoomId room, const void *locker)
{
    // REVISIT: why do we allow locker to be null?
    owners.insert(room);
    if (lockers[room].empty()) {
        holdCount[room] = 0;
    }
    lockers[room].insert(locker);
    ++holdCount[room];
}

void RoomSignalHandler::release(const RoomId room)
{
    assert(holdCount[room]);
    if (--holdCount[room] == 0) {
        if (owners.contains(room)) {
            for (const void *locker_ptr : lockers[room]) {
                if (locker_ptr) { // Ensure locker_ptr is not null before using
                    m_map.releaseRoom(locker_ptr, room);
                }
            }
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

    static_assert(static_cast<uint32_t>(ExitDirEnum::UNKNOWN) + 1 == NUM_EXITS);
    if (isNESWUD(dir) || dir == ExitDirEnum::UNKNOWN) {
        changes.add(exit_change_types::ModifyExitConnection{ChangeTypeEnum::Add,
                                                            fromId,
                                                            dir,
                                                            room,
                                                            WaysEnum::OneWay});
    }

    if (!lockers[room].empty()) {
        const void *first_locker = *(lockers[room].begin());
        if (first_locker) { // Ensure first_locker is not null
            m_map.keepRoom(first_locker, room);
            lockers[room].erase(first_locker);
        } else {
            assert(false); // Or handle null locker appropriately
        }
    }
    release(room);
}
