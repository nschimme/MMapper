#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2021 The MMapper Authors

#include "Connections.h"
#include "RoomTint.h"

#include <map>

template<typename T>
using RoomTintArray = EnumIndexedArray<T, RoomTintEnum, NUM_ROOM_TINTS>;

struct NODISCARD ChunkId final
{
    int x = 0;
    int y = 0;
    int z = 0;

    ChunkId() = default;
    ChunkId(int x_, int y_, int z_)
        : x(x_)
        , y(y_)
        , z(z_)
    {}

    bool operator<(const ChunkId &other) const
    {
        if (z != other.z)
            return z < other.z;
        if (y != other.y)
            return y < other.y;
        return x < other.x;
    }

    bool operator==(const ChunkId &other) const
    {
        return x == other.x && y == other.y && z == other.z;
    }

    bool operator!=(const ChunkId &other) const { return !(*this == other); }
};

inline ChunkId getChunkId(const Coordinate &c, int chunkSize)
{
    return ChunkId{(c.x >= 0) ? (c.x / chunkSize) : ((c.x - chunkSize + 1) / chunkSize),
                   (c.y >= 0) ? (c.y / chunkSize) : ((c.y - chunkSize + 1) / chunkSize),
                   c.z};
}

struct NODISCARD LayerMeshes final
{
    UniqueMeshVector terrain;
    UniqueMeshVector trails;
    RoomTintArray<UniqueMesh> tints;
    UniqueMeshVector overlays;
    UniqueMeshVector doors;
    UniqueMeshVector walls;
    UniqueMeshVector dottedWalls;
    UniqueMeshVector upDownExits;
    UniqueMeshVector streamIns;
    UniqueMeshVector streamOuts;
    UniqueMesh layerBoost;
    bool isValid = false;

    LayerMeshes() = default;
    DEFAULT_MOVES_DELETE_COPIES(LayerMeshes);
    ~LayerMeshes() = default;

    void render(int thisLayer, int focusedLayer);
    explicit operator bool() const { return isValid; }
};

struct NODISCARD LayerMeshesIntermediate final
{
    using Fn = std::function<UniqueMesh(OpenGL &)>;
    using FnVec = std::vector<Fn>;
    FnVec terrain;
    FnVec trails;
    RoomTintArray<Fn> tints;
    FnVec overlays;
    FnVec doors;
    FnVec walls;
    FnVec dottedWalls;
    FnVec upDownExits;
    FnVec streamIns;
    FnVec streamOuts;
    Fn layerBoost;
    bool isValid = false;

    LayerMeshesIntermediate() = default;
    DEFAULT_MOVES_DELETE_COPIES(LayerMeshesIntermediate);
    ~LayerMeshesIntermediate() = default;

    NODISCARD LayerMeshes getLayerMeshes(OpenGL &gl) const;
    explicit operator bool() const { return isValid; }
};

// This must be ordered so we can iterate over the layers from lowest to highest.
using BatchedMeshes = std::map<ChunkId, LayerMeshes>;

struct NODISCARD MapBatches final
{
    BatchedMeshes batchedMeshes;
    BatchedConnectionMeshes connectionMeshes;
    BatchedRoomNames roomNameBatches;

    MapBatches() = default;
    ~MapBatches() = default;
    DEFAULT_MOVES_DELETE_COPIES(MapBatches);
};
