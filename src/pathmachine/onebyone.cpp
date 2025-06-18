// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "onebyone.h"

#include "../global/utils.h"
#include "../map/CommandId.h"
#include "../map/Compare.h"
#include "../map/MapFrontend.h" // For handler->m_map
#include "../map/parseevent.h"
#include "../map/room.h"
#include "experimenting.h" // For PathList::alloc through base
#include "pathparameters.h"
#include "roomsignalhandler.h" // For RoomSignalHandler, provides access to MapFrontend

#include <list>
#include <memory>

// Path class is included via onebyone.h

OneByOne::OneByOne(const SigParseEvent &sigParseEvent,
                   PathParameters &in_params,
                   RoomSignalHandler *const in_handler)
    : Experimenting{PathList::alloc(), getDirection(sigParseEvent.deref().getMoveType()), in_params}
    , event{sigParseEvent.getShared()}
    , handler{in_handler}
    , m_new_paths{PathList::alloc()} // Initialize m_new_paths
// m_path is default-initialized (nullptr)
// m_collectedRoomIds is default-initialized (empty)
{}

// virt_receiveRoom is removed. New receiveRoom methods below.

void OneByOne::receiveRoom(const RoomHandle &room) {
    // params is a member of the base class Experimenting
    // event is a member of OneByOne
    // matches function is assumed to exist and work like ::compare for boolean result
    if (room.exists() && matches(room.getRaw(), event.deref(), params.matchingTolerance)) {
        m_collectedRoomIds.insert(room.getId());
    }
}

void OneByOne::receiveRoom(ServerRoomId server_id) {
    if (server_id != INVALID_SERVER_ROOMID) {
        // handler is a member of OneByOne, providing m_map access
        if (const auto rh = handler->m_map.findRoomHandle(server_id)) {
            if (rh.exists() && matches(rh.getRaw(), event.deref(), params.matchingTolerance)) {
                m_collectedRoomIds.insert(rh.getId());
            }
        }
    }
}

void OneByOne::addPath(const std::shared_ptr<Path> &path_param) {
    m_path = path_param;
    m_collectedRoomIds.clear(); // Clear for the new parent path
}

PathListPtr OneByOne::evaluate() {
    // m_new_paths is a member std::shared_ptr<PathList>, initialized in constructor.
    // Clear it for the current evaluation pass.
    m_new_paths->clear();

    if (!m_path || m_path->isDenied()) {
        return m_new_paths;
    }

    for (RoomId roomId : m_collectedRoomIds) {
        RoomHandle roomHandle = handler->m_map.findRoomHandle(roomId);
        if (!roomHandle.exists()) {
            continue;
        }

        // params is from Experimenting base class
        if (m_new_paths->size() >= static_cast<size_t>(params.maxPaths)) {
            // maxPaths is defined in PathParameters, accessible via params
            break;
        }

        // event is a member of OneByOne
        // m_path is the current parent path, set by addPath()
        // handler is used as the signaler/QObject parent for the new Path
        auto p = Path::alloc(roomHandle, m_path.get(), handler, event.deref().getMoveType());
        m_path->addChild(p);
        m_new_paths->push_back(p);
    }
    return m_new_paths;
}
