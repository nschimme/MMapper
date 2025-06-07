#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2021 The MMapper Authors

// Moved ChunkId definition to the top to ensure it's available early.
// This helps resolve potential include order issues.
using ChunkId = int;
// #include <cstdint> // If ChunkId were, e.g., int32_t, this might be needed here. For `int`, it's fundamental.

#include <array> // For std::array
#include "../map/mmapper2room.h" // For NUM_ROOM_TINTS and TintIndices
#include "Connections.h"
#include "MapCanvasData.h"

#include <map>

// Restoring RoomTintArray using std::array and NUM_ROOM_TINTS
template<typename T>
using RoomTintArray = std::array<T, NUM_ROOM_TINTS>;

struct NODISCARD LayerMeshes final
{
    UniqueMeshVector terrain;
    UniqueMeshVector trails;
    RoomTintArray<UniqueMesh> tints; // Restored
    // UniqueMesh darkTintMesh;        // Removed
    // UniqueMesh noSundeathTintMesh;  // Removed
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

// This must be ordered so we can iterate over the layers from lowest to highest.
using ChunkedLayerMeshes = std::map<ChunkId, LayerMeshes>;

struct NODISCARD MapBatches final
{
    std::map<int, ChunkedLayerMeshes> batchedMeshes;
    std::map<int, std::map<ChunkId, ConnectionMeshes>> connectionMeshes; // Updated
    std::map<int, std::map<ChunkId, UniqueMesh>> roomNameBatches; // Updated

    MapBatches() = default;
    ~MapBatches() = default;
    DEFAULT_MOVES_DELETE_COPIES(MapBatches);
};
