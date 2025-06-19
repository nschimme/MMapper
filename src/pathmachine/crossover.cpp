// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "crossover.h"

#include "../global/utils.h" // Added for deref
#include "../map/ChangeTypes.h" // Added for room_change_types
#include "../map/ExitDirection.h"
#include "../map/room.h"
#include "../mapdata/mapdata.h"
#include "experimenting.h"
// #include "pathmachine.h" // No longer needed for ParseEvent here

#include <memory>

struct PathParameters;

Crossover::Crossover(MapFrontend &map,
                     std::shared_ptr<PathList> _paths,
                     const ExitDirEnum _dirCode,
                     PathParameters &_params)
    : Experimenting(std::move(_paths), _dirCode, _params)
    , m_map{map}
{}

void Crossover::processRoom(const RoomHandle &room) // Removed ParseEvent parameter
{
    // Logic from former Crossover::virt_receiveRoom
    // The 'shortPaths' member is from the Experimenting base class.
    // It's initialized by the Experimenting constructor with the 'paths' passed to Crossover's constructor.
    if (deref(shortPaths).empty()) {
        if (auto rh = m_map.findRoomHandle(room.getId())) {
            if (rh.isTemporary()) {
                // This part implies that if Crossover is fed a room when it has no initial paths to work on,
                // and that room is temporary, it should be removed.
                // This might be a cleanup mechanism or a specific edge case handling.
                m_map.applySingleChange(Change{room_change_types::RemoveRoom{room.getId()}});
            }
        }
        // If shortPaths is empty, there are no paths to augment, so further processing is skipped.
        return;
    }

    for (const auto &path : deref(shortPaths)) { // Iterate over the initial set of paths
        augmentPath(path, room);
    }
}
