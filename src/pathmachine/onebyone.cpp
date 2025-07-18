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

class Path;

OneByOne::OneByOne(const SigParseEvent &sigParseEvent,
                   PathParameters &in_params,
                   RoomSignalHandler &in_handler)
    : Experimenting{PathList::alloc(), getDirection(sigParseEvent.deref().getMoveType()), in_params}
    , m_event{sigParseEvent.getShared()}
    , m_handler{in_handler}
{}

void OneByOne::virt_receiveRoom(const RoomHandle &room)
{
    if (::compare(room.getRaw(), deref(m_event), m_params.matchingTolerance)
        == ComparisonResultEnum::EQUAL) {
        augmentPath(m_shortPaths->back(), room);
    } else {
        m_handler.hold(room.getId());
        m_handler.release(room.getId());
    }
}

void OneByOne::addPath(std::shared_ptr<Path> path)
{
    m_shortPaths->emplace_back(std::move(path));
}
