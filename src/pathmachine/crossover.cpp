// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "crossover.h"

#include "../map/Compare.h"      // For matches()
#include "../map/Coordinate.h"   // For exitDir() and Coordinate
#include "../map/ExitDirection.h" // For ExitDirEnum and exitDir()
#include "../map/MapFrontend.h"   // For MapFrontend
#include "../map/room.h"
#include "../mapdata/mapdata.h" // May not be needed directly
#include "experimenting.h"
#include "pathparameters.h"      // For PathParameters
#include "roomsignalhandler.h"   // For RoomSignalHandler

#include <memory>

// PathParameters struct forward declaration is not needed as pathparameters.h is included.

Crossover::Crossover(MapFrontend &map_param,
                     std::shared_ptr<PathList> paths_param,
                     const ExitDirEnum dir_code_param,
                     PathParameters &params_param,
                     const SharedParseEvent &event_param,
                     RoomSignalHandler *handler_param)
    : Experimenting(std::move(paths_param), dir_code_param, params_param)
    , m_map{map_param}
    , m_event{event_param}
    , m_room_signal_handler{handler_param}
// m_collectedRoomIds is default-initialized
{}

// virt_receiveRoom is removed. New receiveRoom methods below.

void Crossover::receiveRoom(const RoomHandle &room) {
    // params is from Experimenting base. m_event is Crossover member.
    if (room.exists() && matches(room.getRaw(), m_event.deref(), params.matchingTolerance)) {
        m_collectedRoomIds.insert(room.getId());
    }
}

void Crossover::receiveRoom(ServerRoomId server_id) {
    if (server_id != INVALID_SERVER_ROOMID) {
        // m_map is a Crossover member, m_event is Crossover member, params from base.
        if (const auto rh = m_map.findRoomHandle(server_id)) {
            if (rh.exists() && matches(rh.getRaw(), m_event.deref(), params.matchingTolerance)) {
                m_collectedRoomIds.insert(rh.getId());
            }
        }
    }
}

PathListPtr Crossover::evaluate() {
    auto resulting_paths = PathList::alloc();
    // dirCode is inherited from Experimenting.
    const Coordinate expectedDelta = exitDir(dirCode);

    for (RoomId roomId : m_collectedRoomIds) {
        RoomHandle roomHandle = m_map.findRoomHandle(roomId); // Use Crossover's m_map
        if (!roomHandle.exists()) {
            continue;
        }

        // paths member is inherited from Experimenting base class
        for (const auto& parentPathCandidate : deref(paths)) {
            if (parentPathCandidate->isDenied() || !parentPathCandidate->getRoom().exists()) {
                continue;
            }

            if (roomHandle.getPosition() == parentPathCandidate->getRoom().getPosition() + expectedDelta) {
                // params is inherited from Experimenting. maxPaths is part of PathParameters.
                if (resulting_paths->size() >= static_cast<size_t>(params.maxPaths)) {
                    return resulting_paths;
                }
                // m_event is Crossover member. m_room_signal_handler is Crossover member.
                auto newPath = Path::alloc(roomHandle, parentPathCandidate.get(), m_room_signal_handler, m_event.deref().getMoveType());
                parentPathCandidate->addChild(newPath);
                resulting_paths->push_back(newPath);
                break;
            }
        }
    }
    return resulting_paths;
}
