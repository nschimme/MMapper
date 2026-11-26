#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/ImmUnorderedSet.h"
#include "../global/enums.h"
#include "../global/macros.h"
#include "ExitDirection.h"
#include "Map.h"
#include "RoomIdSet.h"
#include "roomid.h"

#include <optional>
#include <set>
#include <tuple>

// What an Explore-mode player is allowed to see of the map. Pure functions of plain values:
// nothing here knows about GMCP, the observer, or rendering, so every consumer (canvas mask,
// labels, dialogs, path finding) can share one definition and tests need no UI.
//
// Everything not listed as visible or frontier is hidden, so a room nobody thought of fails
// closed by being absent.

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

struct NODISCARD ExplorerVisibilityInput final
{
    Map map;
    // Server IDs the character knows (history, list chunks, live adds).
    ImmUnorderedSet<ServerRoomId> known;
    DiscoveredExits discoveredExits;
    // The player's current room: always visible, even when it is temporary or unknown.
    std::optional<RoomId> currentRoom;
    // Rooms without a server ID that the path machine placed the player in this session. Such
    // rooms have no stable key, so they are only ever visible for the session.
    RoomIdSet placedRooms;
};

struct NODISCARD ExplorerVisibility final
{
    RoomIdSet visible;
    // Not visible, but one step beyond a visible room through an effective exit.
    RoomIdSet frontier;
};

// A hidden exit counts as no exit until discovered. It is discovered when its own side was
// found, or when the room it leads to has a found exit leading back (the other side of the
// same secret door). A hidden exit on a room without a server ID can only be opened from the
// other side.
NODISCARD extern bool isEffectiveExit(const Map &map,
                                      const RoomHandle &room,
                                      ExitDirEnum dir,
                                      const DiscoveredExits &discovered);

NODISCARD extern ExplorerVisibility computeExplorerVisibility(const ExplorerVisibilityInput &in);

namespace test {
extern void testExplorerVisibility();
} // namespace test
