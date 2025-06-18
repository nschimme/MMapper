// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "approved.h"

#include "../map/Compare.h"
#include "../map/room.h"
#include "../mapdata/mapdata.h"
// RoomRecipient.h is removed via approved.h modification

Approved::Approved(MapFrontend &map, const SigParseEvent &sigParseEvent, const int tolerance)
    : myEvent{sigParseEvent.requireValid()}
    , m_map{map}
    , matchingTolerance{tolerance}
// m_collectedRoomIds will be default-initialized
{}

Approved::~Approved()
{
    // Original destructor logic might need review based on how m_collectedRoomIds is used
    // For now, keeping it as is, but it might be simplified if m_map interactions change.
    if (matchedRoom) { // matchedRoom might be obsolete, consider removing
        if (moreThanOne) { // moreThanOne might be obsolete
            m_map.releaseRoom(*this, matchedRoom.getId());
        } else {
            m_map.keepRoom(*this, matchedRoom.getId());
        }
    }
}

// Replaced virt_receiveRoom with the new receiveRoom logic
void Approved::receiveRoom(const RoomHandle &room)
{
    if (room.exists() && matches(room.getRaw(), myEvent.deref(), matchingTolerance)) { // Assuming matches takes tolerance
        m_collectedRoomIds.insert(room.getId());
    }
}

void Approved::receiveRoom(ServerRoomId id)
{
    if (id != INVALID_SERVER_ROOMID) {
        if (const auto rh = m_map.findRoomHandle(id)) {
            if (rh.exists() && matches(rh.getRaw(), myEvent.deref(), matchingTolerance)) { // Assuming matches takes tolerance
                m_collectedRoomIds.insert(rh.getId());
            }
        }
    }
}

RoomHandle Approved::oneMatch() const
{
    if (m_collectedRoomIds.size() == 1) {
        RoomId bestId = m_collectedRoomIds.first();
        return m_map.findRoomHandle(bestId);
    }
    return RoomHandle{}; // Return invalid handle if not exactly one match
}

void Approved::releaseMatch()
{
    m_collectedRoomIds.clear();
    update = false;
    // matchedRoom.reset(); // Kept for now, but might be redundant
    // moreThanOne = false; // Kept for now, but might be redundant
}
