#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/hash.h"
#include "../map/ExitDirection.h"
#include "../map/Map.h"
#include "../map/RoomHandle.h"
#include "../map/roomid.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>

struct NODISCARD DirectedExitKey final
{
    RoomId srcRoom;
    ExitDirEnum dir = ExitDirEnum::UNKNOWN;

    NODISCARD bool operator==(const DirectedExitKey &other) const
    {
        return srcRoom == other.srcRoom && dir == other.dir;
    }
    NODISCARD bool operator!=(const DirectedExitKey &other) const
    {
        return !(*this == other);
    }
};

struct NODISCARD DirectedExitKeyHash final
{
    NODISCARD size_t operator()(const DirectedExitKey &key) const noexcept
    {
        size_t seed = 0;
        hash_combine(seed, key.srcRoom.asUint32());
        hash_combine(seed, static_cast<uint32_t>(key.dir));
        return seed;
    }
};

struct NODISCARD MazeInfo final
{
    // Set of all room IDs that belong to any detected maze cluster
    std::unordered_set<RoomId> mazeRooms;

    // Mapping from room ID to its cluster ID
    std::unordered_map<RoomId, uint32_t> roomClusterId;

    // Self-loops (exits where src == dst)
    std::unordered_set<DirectedExitKey, DirectedExitKeyHash> selfLoops;

    // Internal maze connections (both src and dst belong to the same maze cluster)
    std::unordered_set<DirectedExitKey, DirectedExitKeyHash> internalMazeConnections;

    // Exit connections leading out of the maze (src belongs to cluster M, dst does NOT belong to M)
    std::unordered_set<DirectedExitKey, DirectedExitKeyHash> mazeExitConnections;

    NODISCARD bool isMazeRoom(const RoomId id) const { return mazeRooms.contains(id); }

    NODISCARD bool isMazeExitConnection(const RoomId src, const ExitDirEnum dir) const
    {
        return mazeExitConnections.contains(DirectedExitKey{src, dir});
    }

    NODISCARD bool isInternalMazeConnection(const RoomId src, const ExitDirEnum dir) const
    {
        return internalMazeConnections.contains(DirectedExitKey{src, dir});
    }

    NODISCARD bool isSelfLoop(const RoomId src, const ExitDirEnum dir) const
    {
        return selfLoops.contains(DirectedExitKey{src, dir});
    }
};

class NODISCARD MazeDetector final
{
public:
    NODISCARD static MazeInfo detectMazes(const Map &map);
};
