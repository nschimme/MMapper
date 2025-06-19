// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "approved.h"

#include "../map/Compare.h"
#include "../map/room.h"
#include "../mapdata/mapdata.h"

Approved::Approved(MapFrontend &map, const SigParseEvent &sigParseEvent, const int tolerance)
    : m_myEvent{sigParseEvent.requireValid()} // Prefixed
    , m_map{map}
    , m_matchingTolerance{tolerance} // Prefixed

{}

void Approved::virt_receiveRoom(const RoomHandle &perhaps, ChangeList &changes)
{
    auto &event = m_myEvent.deref(); // Prefixed

    const auto id = perhaps.getId();
    const auto cmp = [this, &event, &id, &perhaps]() {
        // Cache comparisons because we regularly call releaseMatch() and try the same rooms again
        auto it = m_compareCache.find(id); // Prefixed
        if (it != m_compareCache.end()) { // Prefixed
            return it->second;
        }
        const auto result = ::compare(perhaps.getRaw(), event, m_matchingTolerance); // Prefixed
        m_compareCache.emplace(id, result); // Prefixed
        return result;
    }();

    if (cmp == ComparisonResultEnum::DIFFERENT) {
        if (auto rh = m_map.findRoomHandle(id)) {
            if (rh.isTemporary()) {
                changes.add(Change{room_change_types::RemoveRoom{id}});
            }
        }
        return;
    }

    if (m_matchedRoom) { // Prefixed
        // m_moreThanOne should only take effect if multiple distinct rooms match
        if (m_matchedRoom.getId() != id) { // Prefixed
            m_moreThanOne = true; // Prefixed
        }
        if (auto rh = m_map.findRoomHandle(id)) {
            if (rh.isTemporary()) {
                changes.add(Change{room_change_types::RemoveRoom{id}});
            }
        }
        return;
    }

    m_matchedRoom = perhaps; // Prefixed
    if (cmp == ComparisonResultEnum::TOLERANCE
        && (event.hasNameDescFlags() || event.hasServerId())) {
        m_update = true; // Prefixed
    } else if (cmp == ComparisonResultEnum::EQUAL) {
        for (const ExitDirEnum dir : ALL_EXITS_NESWUD) {
            const auto toServerId = event.getExitIds()[dir];
            if (toServerId == INVALID_SERVER_ROOMID) {
                continue;
            }
            const auto &e = m_matchedRoom.getExit(dir); // Prefixed
            if (e.exitIsNoMatch()) {
                continue;
            }
            const auto there = m_map.findRoomHandle(toServerId);
            if (!there && !e.exitIsUnmapped()) {
                // New server id
                m_update = true; // Prefixed
            } else if (there && !e.containsOut(there.getId())) {
                // Existing server id
                m_update = true; // Prefixed
            }
        }
    }
}

RoomHandle Approved::oneMatch() const
{
    // Note the logic: If there's more than one, then we return nothing.
    return m_moreThanOne ? RoomHandle{} : m_matchedRoom; // Prefixed
}

void Approved::releaseMatch(ChangeList &changes)
{
    // Release the current candidate in order to receive additional candidates
    if (m_matchedRoom) { // Prefixed
        if (auto rh = m_map.findRoomHandle(m_matchedRoom.getId())) { // Prefixed
            if (rh.isTemporary()) {
                changes.add(Change{room_change_types::RemoveRoom{m_matchedRoom.getId()}}); // Prefixed
            }
        }
    }
    m_update = false; // Prefixed
    m_matchedRoom.reset(); // Prefixed
    m_moreThanOne = false; // Prefixed
}
