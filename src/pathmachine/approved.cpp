// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "approved.h"

#include "../map/Compare.h"
#include "../map/room.h"
#include "../mapdata/mapdata.h"
#include "pathmachine.h" // Ensure PathProcessor is fully defined

Approved::Approved(MapFrontend &map, const SigParseEvent &sigParseEvent, const int tolerance)
    : myEvent{sigParseEvent.requireValid()}
    , m_map{map}
    , matchingTolerance{tolerance}

{}

Approved::~Approved()
{
    if (matchedRoom) {
        if (moreThanOne) {
            if (auto rh = m_map.findRoomHandle(matchedRoom.getId())) {
                if (rh.isTemporary()) {
                    m_map.applySingleChange(Change{room_change_types::RemoveRoom{matchedRoom.getId()}});
                }
            }
        } else {
            if (auto rh = m_map.findRoomHandle(matchedRoom.getId())) {
                if (rh.isTemporary()) {
                    m_map.applySingleChange(Change{room_change_types::MakePermanent{matchedRoom.getId()}});
                }
            }
        }
    }
}

void Approved::processRoom(const RoomHandle &perhaps, const ParseEvent & /* event */)
{
    // The 'event' parameter is unused because myEvent (derived from sigParseEvent in constructor)
    // is used instead. This class seems to operate on a fixed event context.
    auto &cached_event = myEvent.deref();

    const auto id = perhaps.getId();
    const auto cmp = [this, &event, &id, &perhaps]() {
        // Cache comparisons because we regularly call releaseMatch() and try the same rooms again
        auto it = compareCache.find(id);
        if (it != compareCache.end()) {
            return it->second;
        }
        const auto result = ::compare(perhaps.getRaw(), cached_event, matchingTolerance);
        compareCache.emplace(id, result);
        return result;
    }();

    if (cmp == ComparisonResultEnum::DIFFERENT) {
        if (auto rh = m_map.findRoomHandle(id)) {
            if (rh.isTemporary()) {
                m_map.applySingleChange(Change{room_change_types::RemoveRoom{id}});
            }
        }
        return;
    }

    if (matchedRoom) {
        // moreThanOne should only take effect if multiple distinct rooms match
        if (matchedRoom.getId() != id) {
            moreThanOne = true;
        }
        if (auto rh = m_map.findRoomHandle(id)) {
            if (rh.isTemporary()) {
                m_map.applySingleChange(Change{room_change_types::RemoveRoom{id}});
            }
        }
        return;
    }

    matchedRoom = perhaps;
    if (cmp == ComparisonResultEnum::TOLERANCE
        && (cached_event.hasNameDescFlags() || cached_event.hasServerId())) {
        update = true;
    } else if (cmp == ComparisonResultEnum::EQUAL) {
        for (const ExitDirEnum dir : ALL_EXITS_NESWUD) {
            const auto toServerId = cached_event.getExitIds()[dir];
            if (toServerId == INVALID_SERVER_ROOMID) {
                continue;
            }
            const auto &e = matchedRoom.getExit(dir);
            if (e.exitIsNoMatch()) {
                continue;
            }
            const auto there = m_map.findRoomHandle(toServerId);
            if (!there && !e.exitIsUnmapped()) {
                // New server id
                update = true;
            } else if (there && !e.containsOut(there.getId())) {
                // Existing server id
                update = true;
            }
        }
    }
}

RoomHandle Approved::oneMatch() const
{
    // Note the logic: If there's more than one, then we return nothing.
    return moreThanOne ? RoomHandle{} : matchedRoom;
}

void Approved::releaseMatch()
{
    // Release the current candidate in order to receive additional candidates
    if (matchedRoom) {
        if (auto rh = m_map.findRoomHandle(matchedRoom.getId())) {
            if (rh.isTemporary()) {
                m_map.applySingleChange(Change{room_change_types::RemoveRoom{matchedRoom.getId()}});
            }
        }
    }
    update = false;
    matchedRoom.reset();
    moreThanOne = false;
}
