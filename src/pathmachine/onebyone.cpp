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
// #include "roomsignalhandler.h" // Removed

#include <list>
#include <memory>

class Path;

OneByOne::OneByOne(const SigParseEvent &sigParseEvent,
                   PathParameters &in_params)
    : Experimenting{PathList::alloc(), getDirection(sigParseEvent.deref().getMoveType()), in_params}
    , event{sigParseEvent.getShared()}
    // , handler{in_handler} // Removed handler initialization
{}

void OneByOne::processRoom(const RoomHandle &room, const ParseEvent & /*event_param*/)
{
    // The event_param is unused, class uses its member 'event' (SharedParseEvent)
    if (::compare(room.getRaw(), deref(event), params.matchingTolerance)
        == ComparisonResultEnum::EQUAL) {
        // 'shortPaths' is a member of the Experimenting base class.
        // In OneByOne, paths are added to shortPaths via addPath().
        // We assume addPath has been called before processRoom, and shortPaths is not empty.
        // augmentPath takes the *parent* path and the new room.
        if (!shortPaths->empty()) { // Check if parent path (last added to shortPaths) exists
            augmentPath(shortPaths->back(), room);
        }
    } else {
        // The old logic:
        // handler->hold(room.getId(), this);
        // handler->release(room.getId());
        // This was likely to ensure temporary rooms that don't match are cleaned up.
        // With PathMachine managing holds, if a room is temporary and has no more holds
        // (e.g., because a path to it was denied, triggering releaseRoom in PathMachine),
        // it will be removed. PathProcessor implementations generally shouldn't directly
        // manipulate holds like this. If this room was 'held' by a Path that is now
        // being superseded or found incorrect, that Path's denial will trigger the release.
    }
}

void OneByOne::addPath(std::shared_ptr<Path> path)
{
    shortPaths->emplace_back(std::move(path));
}
