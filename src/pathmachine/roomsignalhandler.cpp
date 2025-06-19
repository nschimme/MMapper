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

// Added for PathMachine::PathProcessor - already included by roomsignalhandler.h, but good for clarity
#include "pathmachine.h"

RoomSignalHandler::RoomSignalHandler(MapFrontend &map)
    : m_map{map}
{}

void RoomSignalHandler::hold(const RoomId room, PathMachine::PathProcessor *const locker)
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
            for (auto i = lockers[room].begin(); i != lockers[room].end(); ++i) {
                if (PathMachine::PathProcessor *const processor = *i) { // CHANGED
                    m_map.releaseRoom(*processor, room); // CHANGED
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
        if (PathMachine::PathProcessor *const processor = *(lockers[room].begin())) { // CHANGED
            m_map.keepRoom(*processor, room); // CHANGED
            lockers[room].erase(processor); // CHANGED (variable name)
        } else {
            assert(false);
        }
    }
    release(room);
}
