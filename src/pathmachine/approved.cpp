// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "approved.h"

#include "../map/Compare.h"
#include "../map/room.h"
#include "../mapdata/mapdata.h"
#include "patheventcontext.h" // Ensure this is included for mmapper::PathEventContext

// Approved::Approved(MapFrontend &map, const SigParseEvent &sigParseEvent, const int tolerance)
Approved::Approved(mmapper::PathEventContext &context, const int tolerance)
    // : m_myEvent{sigParseEvent.requireValid()} // Removed
    // , m_map{map} // Removed
    : m_context{context} // Added
    , m_matchingTolerance{tolerance}
{}

Approved::~Approved()
{
    if (m_matchedRoom) {
        if (m_moreThanOne) {
            // if (auto rh = m_map.findRoomHandle(m_matchedRoom.getId())) { becomes:
            if (auto rh = m_context.map.findRoomHandle(m_matchedRoom.getId())) {
                if (rh.isTemporary()) {
                    // Use addTrackedChange
                    m_context.addTrackedChange(
                        Change{room_change_types::RemoveRoom{m_matchedRoom.getId()}});
                }
            }
        } else {
            // if (auto rh = m_map.findRoomHandle(m_matchedRoom.getId())) { becomes:
            if (auto rh = m_context.map.findRoomHandle(m_matchedRoom.getId())) {
                if (rh.isTemporary()) {
                    // Use addTrackedChange
                    m_context.addTrackedChange(
                        Change{room_change_types::MakePermanent{m_matchedRoom.getId()}});
                }
            }
        }
    }
}

void Approved::virt_receiveRoom(const RoomHandle &perhaps)
{
    // auto &event = m_myEvent.deref(); becomes:
    auto &event = m_context.currentEvent;

    const auto id = perhaps.getId();
    const auto cmp = [this, &event, &id, &perhaps]() {
        // Cache comparisons because we regularly call releaseMatch() and try the same rooms again
        auto it = m_compareCache.find(id);
        if (it != m_compareCache.end()) {
            return it->second;
        }
        // Pass m_matchingTolerance which is a member
        const auto result = ::compare(perhaps.getRaw(), event, m_matchingTolerance);
        m_compareCache.emplace(id, result);
        return result;
    }();

    if (cmp == ComparisonResultEnum::DIFFERENT) {
        // if (auto rh = m_map.findRoomHandle(id)) { becomes:
        if (auto rh = m_context.map.findRoomHandle(id)) {
            if (rh.isTemporary()) {
                // Use addTrackedChange
                m_context.addTrackedChange(Change{room_change_types::RemoveRoom{id}});
            }
        }
        return;
    }

    if (m_matchedRoom) {
        // moreThanOne should only take effect if multiple distinct rooms match
        if (m_matchedRoom.getId() != id) {
            m_moreThanOne = true;
        }
        // if (auto rh = m_map.findRoomHandle(id)) { becomes:
        if (auto rh = m_context.map.findRoomHandle(id)) {
            if (rh.isTemporary()) {
                // Use addTrackedChange
                m_context.addTrackedChange(Change{room_change_types::RemoveRoom{id}});
            }
        }
        return;
    }

    m_matchedRoom = perhaps;
    if (cmp == ComparisonResultEnum::TOLERANCE
        && (event.hasNameDescFlags() || event.hasServerId())) {
        m_update = true;
    } else if (cmp == ComparisonResultEnum::EQUAL) {
        for (const ExitDirEnum dir : ALL_EXITS_NESWUD) {
            const auto toServerId = event.getExitIds()[dir];
            if (toServerId == INVALID_SERVER_ROOMID) {
                continue;
            }
            const auto &e = m_matchedRoom.getExit(dir);
            if (e.exitIsNoMatch()) {
                continue;
            }
            // const auto there = m_map.findRoomHandle(toServerId); becomes:
            const auto there = m_context.map.findRoomHandle(toServerId);
            if (!there && !e.exitIsUnmapped()) {
                // New server id
                m_update = true;
            } else if (there && !e.containsOut(there.getId())) {
                // Existing server id
                m_update = true;
            }
        }
    }
}

RoomHandle Approved::oneMatch() const
{
    // Note the logic: If there's more than one, then we return nothing.
    return m_moreThanOne ? RoomHandle{} : m_matchedRoom;
}

void Approved::releaseMatch()
{
    // Release the current candidate in order to receive additional candidates
    if (m_matchedRoom) {
        // if (auto rh = m_map.findRoomHandle(m_matchedRoom.getId())) { becomes:
        if (auto rh = m_context.map.findRoomHandle(m_matchedRoom.getId())) {
            if (rh.isTemporary()) {
                // Use addTrackedChange
                m_context.addTrackedChange(
                    Change{room_change_types::RemoveRoom{m_matchedRoom.getId()}});
            }
        }
    }
    m_update = false;
    m_matchedRoom.reset();
    m_moreThanOne = false;
}
