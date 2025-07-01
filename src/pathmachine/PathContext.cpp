// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "PathContext.h"
#include "../mapfrontend/mapfrontend.h" // For MapFrontend actual definition
#include "../map/ChangeList.h"
#include "../map/Changes.h"       // For change_types
#include "../map/coordinate.h"   // For Coordinate
#include "../map/parseevent.h"   // For SigParseEvent

PathContext::PathContext(MapFrontend& mapFrontend)
    : m_mapFrontend(mapFrontend)
{
}

std::optional<RoomId> PathContext::requestCreateRoom(const SigParseEvent& event, const Coordinate& coord)
{
    std::optional<RoomId> roomIdOpt = m_mapFrontend.getOrCreateRoomId(event, coord);
    if (roomIdOpt) {
        m_roomsCreatedThisCycle.insert(*roomIdOpt);
    }
    return roomIdOpt;
}

void PathContext::requestDeleteTemporaryRoom(RoomId id)
{
    m_roomsPendingDeletion.insert(id);
    // If it was pending to be made permanent, that's now overridden.
    m_roomsPendingPermanent.erase(id);
}

void PathContext::requestMakeRoomPermanent(RoomId id)
{
    m_roomsPendingPermanent.insert(id);
    // If it was pending deletion, that's now overridden.
    m_roomsPendingDeletion.erase(id);
}

bool PathContext::isRoomPendingDeletion(RoomId id) const
{
    return m_roomsPendingDeletion.count(id);
}

bool PathContext::isRoomPendingPermanent(RoomId id) const
{
    return m_roomsPendingPermanent.count(id);
}

bool PathContext::wasRoomCreatedThisCycle(RoomId id) const
{
    return m_roomsCreatedThisCycle.count(id);
}

RoomHandle PathContext::findRoomHandle(RoomId id) const
{
    return m_mapFrontend.findRoomHandle(id);
}

RoomHandle PathContext::findRoomHandle(const Coordinate& coord) const
{
    return m_mapFrontend.findRoomHandle(coord);
}

RoomHandle PathContext::findRoomHandle(ServerRoomId id) const
{
    return m_mapFrontend.findRoomHandle(id);
}

void PathContext::flushChanges(ChangeList& outChangeList)
{
    // Process deletions first
    for (RoomId id : m_roomsPendingDeletion) {
        RoomHandle roomHandle = m_mapFrontend.findRoomHandle(id);
        if (roomHandle) { // Check if room still exists
            // Only request deletion if it's a temporary room.
            // A room marked for deletion that was also marked for permanent (and was created this cycle)
            // should effectively not be made permanent and then deleted,
            // but rather just not made permanent if it was new.
            // If it's an existing permanent room, requestDeleteTemporaryRoom should ideally not be called,
            // or this check should be more robust (e.g. hasTemporaryRoom(id) on MapFrontend).
            if (roomHandle.isTemporary() || wasRoomCreatedThisCycle(id)) {
                 // If it was created this cycle AND marked for permanent, but also for delete,
                 // deletion wins, and it shouldn't have been made permanent.
                 // If it was created this cycle and NOT marked for permanent, it's a temp room.
                outChangeList.add(Change{room_change_types::RemoveRoom{id}});
            }
        }
        // If room doesn't exist, no action to take for deletion.
    }

    // Process making rooms permanent
    for (RoomId id : m_roomsPendingPermanent) {
        if (m_roomsPendingDeletion.count(id)) {
            // If room is also marked for deletion, deletion takes precedence. Don't make it permanent.
            continue;
        }

        RoomHandle roomHandle = m_mapFrontend.findRoomHandle(id);
        if (roomHandle) { // Check if room still exists
            // Make permanent if it was created this cycle (and thus is temporary by default from AddRoom2)
            // or if it's an existing temporary room.
            if (wasRoomCreatedThisCycle(id) || roomHandle.isTemporary()) {
                outChangeList.add(Change{room_change_types::MakePermanent{id}});
            }
        }
        // If room doesn't exist (e.g. deleted by another process, though unlikely here), cannot make permanent.
    }

    // Clear the tracking sets for the next cycle
    m_roomsCreatedThisCycle.clear();
    m_roomsPendingDeletion.clear();
    m_roomsPendingPermanent.clear();
}
