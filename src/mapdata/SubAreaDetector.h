#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/hash.h"
#include "../map/ExitDirection.h"
#include "../map/Map.h"
#include "../map/RoomHandle.h"
#include "../map/SubAreaFlags.h"
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
    NODISCARD bool operator!=(const DirectedExitKey &other) const { return !(*this == other); }
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

struct NODISCARD SubAreaInfo final
{
    // Mapping from room ID to its detected sub-area cluster ID
    std::unordered_map<RoomId, uint32_t> roomClusterId;

    // Mapping from room ID to its detected SubAreaFlags
    std::unordered_map<RoomId, SubAreaFlags> roomFlags;

    // Self-loops (exits where src == dst)
    std::unordered_set<DirectedExitKey, DirectedExitKeyHash> selfLoops;

    // Internal sub-area connections (both src and dst belong to the same cluster)
    std::unordered_set<DirectedExitKey, DirectedExitKeyHash> internalConnections;

    // Boundary exit connections (src belongs to cluster C, dst does NOT belong to C)
    std::unordered_set<DirectedExitKey, DirectedExitKeyHash> boundaryExitConnections;

    // Chokepoint exit connections
    std::unordered_set<DirectedExitKey, DirectedExitKeyHash> chokepointConnections;

    NODISCARD bool isSubAreaRoom(const RoomId id) const { return roomClusterId.contains(id); }

    NODISCARD SubAreaFlags getFlags(const RoomId id) const
    {
        auto it = roomFlags.find(id);
        return (it != roomFlags.end()) ? it->second : SubAreaFlags{};
    }

    NODISCARD bool isMazeRoom(const RoomId id) const
    {
        auto flags = getFlags(id);
        return flags.contains(SubAreaFlagEnum::MAZE);
    }

    NODISCARD bool isBoundaryExitConnection(const RoomId src, const ExitDirEnum dir) const
    {
        return boundaryExitConnections.contains(DirectedExitKey{src, dir});
    }

    NODISCARD bool isMazeExitConnection(const RoomId src, const ExitDirEnum dir) const
    {
        return isBoundaryExitConnection(src, dir);
    }

    NODISCARD bool isInternalConnection(const RoomId src, const ExitDirEnum dir) const
    {
        return internalConnections.contains(DirectedExitKey{src, dir});
    }

    NODISCARD bool isInternalMazeConnection(const RoomId src, const ExitDirEnum dir) const
    {
        return isInternalConnection(src, dir);
    }

    NODISCARD bool isSelfLoop(const RoomId src, const ExitDirEnum dir) const
    {
        return selfLoops.contains(DirectedExitKey{src, dir});
    }

    NODISCARD bool isChokepointConnection(const RoomId src, const ExitDirEnum dir) const
    {
        return chokepointConnections.contains(DirectedExitKey{src, dir});
    }
};

class NODISCARD SubAreaDetector final
{
public:
    NODISCARD static SubAreaInfo detectSubAreas(const Map &map);
};

// Compatibility wrapper for MazeDetector if needed
using MazeInfo = SubAreaInfo;
class NODISCARD MazeDetector final
{
public:
    NODISCARD static MazeInfo detectMazes(const Map &map)
    {
        return SubAreaDetector::detectSubAreas(map);
    }
};
