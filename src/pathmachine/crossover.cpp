// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "crossover.h"

#include "../map/ExitDirection.h"
#include "../map/room.h"
#include "../mapdata/mapdata.h"
#include "experimenting.h"
#include "patheventcontext.h" // For mmapper::PathEventContext

#include <memory>

struct PathParameters;

// Crossover::Crossover(MapFrontend &map, ...)
Crossover::Crossover(mmapper::PathEventContext &context, // Added context
                     std::shared_ptr<PathList> _paths,
                     const ExitDirEnum _dirCode,
                     PathParameters &_params)
    // Pass context to Experimenting constructor
    : Experimenting(context, std::move(_paths), _dirCode, _params)
    // , m_map{map} // Removed m_map member
{}

void Crossover::virt_receiveRoom(const RoomHandle &room)
{
    auto &shortPaths = deref(m_shortPaths); // m_shortPaths is from Experimenting
    if (shortPaths.empty()) {
        // if (auto rh = m_map.findRoomHandle(room.getId())) { becomes:
        // m_context is inherited from Experimenting
        RoomId currentRoomId = room.getId(); // Define currentRoomId
        if (auto rh = m_context.map.findRoomHandle(currentRoomId)) {
            if (rh.isTemporary()) {
                // Refined: check context before calling addTrackedChange for RemoveRoom
                if (m_context.getPendingOperation(currentRoomId) != mmapper::PendingRoomOperation::PENDING_REMOVE_ROOM) {
                    m_context.addTrackedChange(Change{room_change_types::RemoveRoom{currentRoomId}});
                }
            }
        }
    }

    for (auto &shortPath : shortPaths) {
        augmentPath(shortPath, room);
    }
}
