// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "approved.h"

#include "../map/ChangeTypes.h" // Added ChangeTypes.h
#include "../map/Compare.h"
#include "../map/room.h"
#include "../mapdata/mapdata.h"

Approved::Approved(MapFrontend &map, const SigParseEvent &sigParseEvent, const int tolerance)
    : myEvent{sigParseEvent.requireValid()}
    , m_map{map}
    , matchingTolerance{tolerance}

{}

// Approved::~Approved() // Destructor removed

std::vector<Change> Approved::finalizeChanges()
{
    std::vector<Change> changes;
    if (matchedRoom) {
        // Logic from old destructor
        // Need to find the handle again to check isTemporary, as matchedRoom itself might be stale
        // or not have the most up-to-date temporary status.
        if (auto rh = m_map.findRoomHandle(matchedRoom.getId())) {
            if (rh.isTemporary()) { // Check if the room is currently temporary
                if (moreThanOne) {
                    changes.emplace_back(room_change_types::RemoveRoom{matchedRoom.getId()});
                } else {
                    changes.emplace_back(room_change_types::MakePermanent{matchedRoom.getId()});
                }
            }
        }
    }

    for (const RoomId r_id : m_rooms_to_remove) {
        changes.emplace_back(room_change_types::RemoveRoom{r_id});
    }
    m_rooms_to_remove.clear();
    return changes;
}

void Approved::processRoom(const RoomHandle &perhaps) // Renamed from virt_receiveRoom
{
    auto &event = myEvent.deref();

    const auto id = perhaps.getId();
    const auto cmp = [this, &event, &id, &perhaps]() {
        // Cache comparisons because we regularly call releaseMatch() and try the same rooms again
        auto it = compareCache.find(id);
        if (it != compareCache.end()) {
            return it->second;
        }
        const auto result = ::compare(perhaps.getRaw(), event, matchingTolerance);
        compareCache.emplace(id, result);
        return result;
    }();

    if (cmp == ComparisonResultEnum::DIFFERENT) {
        if (auto rh = m_map.findRoomHandle(id)) {
            if (rh.isTemporary()) {
                // m_map.applySingleChange(Change{room_change_types::RemoveRoom{id}});
                m_rooms_to_remove.push_back(id); // Changed to add to vector
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
                // m_map.applySingleChange(Change{room_change_types::RemoveRoom{id}});
                m_rooms_to_remove.push_back(id); // Changed to add to vector
            }
        }
        return;
    }

    matchedRoom = perhaps;
    if (cmp == ComparisonResultEnum::TOLERANCE
        && (event.hasNameDescFlags() || event.hasServerId())) {
        update = true;
    } else if (cmp == ComparisonResultEnum::EQUAL) {
        for (const ExitDirEnum dir : ALL_EXITS_NESWUD) {
            const auto toServerId = event.getExitIds()[dir];
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
                // m_map.applySingleChange(Change{room_change_types::RemoveRoom{matchedRoom.getId()}});
                m_rooms_to_remove.push_back(matchedRoom.getId()); // Changed to add to vector
            }
        }
    }
    update = false;
    matchedRoom.reset();
    moreThanOne = false;
}
