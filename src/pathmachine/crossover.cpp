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

void Crossover::virt_receiveRoom(const RoomHandle &room, ChangeList &changes)
{
    if (deref(shortPaths).empty()) {
        if (auto rh = m_map.findRoomHandle(room.getId())) {
            if (rh.isTemporary()) {
                changes.add(Change{room_change_types::RemoveRoom{room.getId()}});
            }
        }
    }

    for (auto &shortPath : *shortPaths) {
        augmentPath(shortPath, room);
    }
}
