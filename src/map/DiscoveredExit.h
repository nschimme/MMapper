#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/enums.h"
#include "../global/macros.h"
#include "ExitDirection.h"
#include "roomid.h"

#include <set>
#include <tuple>

// A secret exit the character has found, keyed by the server's stable room ID.
struct NODISCARD DiscoveredExit final
{
    ServerRoomId room;
    ExitDirEnum dir = ExitDirEnum::NONE;

    NODISCARD friend bool operator<(const DiscoveredExit &lhs, const DiscoveredExit &rhs)
    {
        return std::make_tuple(lhs.room.asUint32(), enums::to_underlying(lhs.dir))
               < std::make_tuple(rhs.room.asUint32(), enums::to_underlying(rhs.dir));
    }
    NODISCARD friend bool operator==(const DiscoveredExit &lhs, const DiscoveredExit &rhs)
    {
        return lhs.room == rhs.room && lhs.dir == rhs.dir;
    }
};
using DiscoveredExits = std::set<DiscoveredExit>;
