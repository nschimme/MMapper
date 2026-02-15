#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "../global/EnumIndexedArray.h"
#include "../global/RuleOf5.h"
#include "../global/macros.h"
#include "../map/RoomTint.h"
#include "../map/coordinate.h"
#include "../opengl/OpenGLTypes.h"
#include "Connections.h"

#include <map>
#include <set>
#include <unordered_map>

template<typename T>
using RoomTintArray = EnumIndexedArray<T, RoomTintEnum, NUM_ROOM_TINTS>;

using RoomVector = std::vector<RoomHandle>;
using LayerToRooms = std::map<int, RoomVector>;
using ChunkToLayerToRooms = std::unordered_map<ChunkId, LayerToRooms>;

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

    void render(int thisLayer, int focusedLayer) const;

    // Split rendering into passes to ensure correct layering across chunks
    void renderTerrain(int thisLayer, int focusedLayer) const;
    void renderTints() const;
    void renderDetails(int thisLayer, int focusedLayer) const;
    void renderWalls(int thisLayer, int focusedLayer) const;

    explicit operator bool() const { return isValid; }
};

struct NODISCARD ChunkMeshes final
{
    // layer -> meshes
    std::map<int, LayerMeshes> layers;
    BatchedConnectionMeshes connectionMeshes;
    BatchedRoomNames roomNameBatches;

    ChunkMeshes() = default;
    DEFAULT_MOVES_DELETE_COPIES(ChunkMeshes);
    ~ChunkMeshes() = default;

    NODISCARD bool empty() const
    {
        return layers.empty() && connectionMeshes.empty() && roomNameBatches.empty();
    }
};

using BatchedChunks = std::unordered_map<ChunkId, ChunkMeshes>;

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
