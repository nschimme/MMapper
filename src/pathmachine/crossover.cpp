// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "crossover.h"

#include "../map/ExitDirection.h"
#include "../map/room.h"
#include "../mapdata/mapdata.h"
#include "experimenting.h"

#include <memory>

struct PathParameters;

Crossover::Crossover(MapFrontend &map,
                     std::shared_ptr<PathList> _paths,
                     const ExitDirEnum _dirCode,
                     PathParameters &_params)
    : Experimenting(std::move(_paths), _dirCode, _params)
    , m_map{map}
{}

void Crossover::receiveRoom(const RoomHandle &room)
{
    if (deref(shortPaths).empty()) {
        // If shortPaths is empty, this room is unexpected or not processable by Crossover.
        // PathMachine's call to lookingForRooms would have used a lambda with [&exp]
        // where exp is this Crossover instance. If Crossover decides not to use this room,
        // it doesn't need to explicitly call releaseRoom, as it was never "kept" by this instance.
        // m_map.releaseRoom(this, room.getId()); // This line is removed.
        return; // Added return, as there's nothing to do if shortPaths is empty.
    }

    for (auto &shortPath : *shortPaths) {
        augmentPath(shortPath, room);
    }
}
