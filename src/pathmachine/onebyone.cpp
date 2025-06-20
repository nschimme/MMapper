// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "onebyone.h"

#include "../global/utils.h"
#include "../map/CommandId.h"
#include "../map/Compare.h"
#include "../map/parseevent.h"
#include "../map/room.h"
#include "experimenting.h"
#include "pathparameters.h"
#include "roomsignalhandler.h"

#include <list>
#include <memory>

class Path;

OneByOne::OneByOne(const SigParseEvent &sigParseEvent,
                   PathParameters &in_params,
                   RoomSignalHandler &in_handler) // Parameter name from .h is 'handler' but previous change used in_handler, sticking to in_handler for cpp if it's consistent
    : Experimenting{PathList::alloc(), getDirection(sigParseEvent.deref().getMoveType()), in_params} // Pass in_params to base
    , m_event{sigParseEvent.getShared()} // Prefixed
    , m_handler{in_handler} // Prefixed member initialized with in_handler
{}

void OneByOne::virt_receiveRoom(const RoomHandle &room, ChangeList &changes)
{
    // Accessing m_params inherited from Experimenting (which should be m_params there too)
    if (::compare(room.getRaw(), deref(m_event), this->m_params.matchingTolerance) // Prefixed event, and this->m_params
        == ComparisonResultEnum::EQUAL) {
        // Accessing m_shortPaths inherited from Experimenting
        augmentPath(this->m_shortPaths->back(), room); // Prefixed this->m_shortPaths
    } else {
        // needed because the memory address is not unique and
        // calling admin->release might destroy a room still held by some path
        m_handler.hold(room.getId(), this);
        m_handler.release(room, changes); // Pass the RoomHandle 'room' directly
    }
}

void OneByOne::addPath(std::shared_ptr<Path> path)
{
    // Accessing m_shortPaths inherited from Experimenting
    this->m_shortPaths->emplace_back(std::move(path)); // Prefixed this->m_shortPaths
}

// Removed getSharedPtrFromThis implementations
