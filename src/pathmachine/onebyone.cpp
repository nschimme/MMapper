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

OneByOne::OneByOne(const SigParseEvent &sigParseEvent, // Constructor updated
                   PathParameters &in_params)
    // RoomSignalHandler *const in_handler) // Removed parameter
    : Experimenting{PathList::alloc(), getDirection(sigParseEvent.deref().getMoveType()), in_params}
    , event{sigParseEvent.getShared()}
    // , handler{in_handler} // Removed member initialization
{}

void OneByOne::processRoom(const RoomHandle &room) // Renamed method
{
    if (::compare(room.getRaw(), deref(event), params.matchingTolerance)
        == ComparisonResultEnum::EQUAL) {
        augmentPath(shortPaths->back(), room);
    }
    // else block entirely removed
}

void OneByOne::addPath(std::shared_ptr<Path> path)
{
    shortPaths->emplace_back(std::move(path));
}
