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

#include <memory>

#include "patheventcontext.h" // For mmapper::PathEventContext

class Path;

// OneByOne::OneByOne(const SigParseEvent &sigParseEvent, PathParameters &in_params, RoomSignalHandler &in_handler)
OneByOne::OneByOne(mmapper::PathEventContext &context, // Added context
                   PathParameters &in_params,
                   RoomSignalHandler &in_handler)
    // Pass context to Experimenting, and use context.currentEvent for getDirection
    : Experimenting{context, PathList::alloc(), getDirection(context.currentEvent.getMoveType()), in_params}
    // , m_event{sigParseEvent.getShared()} // Removed m_event member
    , m_handler{in_handler}
{}

void OneByOne::virt_receiveRoom(const RoomHandle &room)
{
    // if (::compare(room.getRaw(), deref(m_event), m_params.matchingTolerance) becomes:
    // m_context is inherited from Experimenting
    if (::compare(room.getRaw(), m_context.currentEvent, m_params.matchingTolerance)
        == ComparisonResultEnum::EQUAL) {
        // m_shortPaths is from Experimenting
        augmentPath(m_shortPaths->back(), room);
    } else {
        // This hold()/release() sequence ensures that if this room was only being kept alive
        // due to this transient interaction, its lifecycle is correctly managed via RoomSignalHandler's
        // reference counting (m_holdCount). It prevents premature cleanup if the room should persist
        // due to other holds, and ensures cleanup if this was the effective last hold.
        m_handler.hold(room.getId());
        m_handler.release(room.getId());
    }
}

void OneByOne::addPath(std::shared_ptr<Path> path)
{
    m_shortPaths->emplace_back(std::move(path));
}
