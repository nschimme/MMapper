#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "../map/DoorFlags.h"
#include "../map/ExitDirection.h"
#include "../map/ExitFieldVariant.h"
#include "../map/ExitFlags.h"
#include "../map/parseevent.h" // For SharedParseEvent
#include "../map/RoomIdSet.h"   // Added
#include "experimenting.h"
#include "path.h"

#include <memory>

class MapFrontend;
class RoomSignalHandler; // Forward declaration
struct PathParameters;

class NODISCARD Crossover final : public Experimenting
{
private:
    MapFrontend &m_map;
    SharedParseEvent m_event; // Added
    RoomSignalHandler *m_room_signal_handler; // Added
    RoomIdSet m_collectedRoomIds; // Added

public:
    Crossover(MapFrontend &map,
              std::shared_ptr<PathList> paths,
              ExitDirEnum dirCode,
              PathParameters &params,
              const SharedParseEvent &event, // Added to constructor
              RoomSignalHandler *room_signal_handler); // Added to constructor

    // virt_receiveRoom removed. Crossover will define its own non-virtual receiveRoom methods.
};
