// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "roomsignalhandler.h"

#include "../mapdata/mapdata.h"

#include <cassert>

void RoomSignalHandler::hold(const RoomId room, PathProcessor *const locker)
{
    // REVISIT: why do we allow locker to be null?
    m_owners.insert(room);
    auto [it, inserted] = m_lockers.try_emplace(room);
    if (inserted) {
        m_holdCount[room] = 0;
    }
    it->second.insert(locker);
    ++m_holdCount[room];
}

void RoomSignalHandler::release(const RoomId room)
{
    auto it_hold_count = m_holdCount.find(room);
    assert(it_hold_count != m_holdCount.end() && it_hold_count->second > 0);

    if (--it_hold_count->second == 0) {
        if (m_owners.contains(room)) {
            if (auto rh = m_map.findRoomHandle(room)) {
                if (rh.isTemporary()) {
                    m_map.applySingleChange(Change{room_change_types::RemoveRoom{room}});
                }
            }
        } else {
            assert(false);
        }

        m_lockers.erase(room);
        m_owners.erase(room);
        m_holdCount.erase(room);
    }
}

void RoomSignalHandler::keep(const RoomId room,
                             const ExitDirEnum dir,
                             const RoomId fromId,
                             ChangeList &changes)
{
    auto it_hold_count = m_holdCount.find(room);
    assert(it_hold_count != m_holdCount.end() && it_hold_count->second != 0);
    assert(m_owners.contains(room));

    static_assert(static_cast<uint32_t>(ExitDirEnum::UNKNOWN) + 1 == NUM_EXITS);
    if (isNESWUD(dir) || dir == ExitDirEnum::UNKNOWN) {
        changes.add(exit_change_types::ModifyExitConnection{ChangeTypeEnum::Add,
                                                            fromId,
                                                            dir,
                                                            room,
                                                            WaysEnum::OneWay});
    }

    if (auto it_lockers = m_lockers.find(room);
        it_lockers != m_lockers.end() && !it_lockers->second.empty()) {
        PathProcessor *const locker = *(it_lockers->second.begin());
        if (locker) {
            if (auto rh = m_map.findRoomHandle(room)) {
                if (rh.isTemporary()) {
                    // REVISIT: Use changes.add() instead of applySingleChange() and release()?
                    m_map.applySingleChange(Change{room_change_types::MakePermanent{room}});
                }
            }
            it_lockers->second.erase(locker);
        } else {
            assert(false);
        }
    }
    release(room);
}
