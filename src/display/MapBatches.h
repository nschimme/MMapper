#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2021 The MMapper Authors

#include "Connections.h"       // For BatchedConnectionMeshes
#include "MapCanvasData.h"     // For UniqueMesh, UniqueMeshVector, BatchedRoomNames etc.
#include "MapCanvasRoomDrawer.h" // For RoomGeometry (MUST BE INCLUDED BEFORE THIS LINE)

#include <map>
#include <unordered_map>
#include <vector>
#include <glm/glm.hpp> // For glm::mat4

// Forward declaration from opengl/OpenGL.h - needed for LayerMeshes::render signature
class OpenGL;

template<typename T>
using RoomTintArray = EnumIndexedArray<T, RoomTintEnum, NUM_ROOM_TINTS>;

// Represents all mesh components for a single unique room geometry
struct RoomComponentMeshes {
    UniqueMesh terrain{}; // Default initialize
    UniqueMesh trails{};  // Default initialize
    UniqueMeshVector overlays{};
    UniqueMeshVector doors{};
    UniqueMeshVector walls{};
    UniqueMeshVector dottedWalls{};
    UniqueMeshVector upDownExits{};
    UniqueMeshVector streamIns{};
    UniqueMeshVector streamOuts{};

    RoomComponentMeshes() = default;
    ~RoomComponentMeshes() = default;
    DEFAULT_MOVES_DELETE_COPIES(RoomComponentMeshes);
};

struct NODISCARD LayerMeshes final
{
    // Original members for non-instanced or layer-wide effects
    UniqueMeshVector terrain; // May become obsolete or used as a fallback for non-instanced items
    UniqueMeshVector trails;  // May become obsolete
    RoomTintArray<UniqueMesh> tints;
    UniqueMeshVector overlays; // May become obsolete
    UniqueMeshVector doors;    // May become obsolete
    UniqueMeshVector walls;    // May become obsolete
    UniqueMeshVector dottedWalls; // May become obsolete
    UniqueMeshVector upDownExits; // May become obsolete
    UniqueMeshVector streamIns;   // May become obsolete
    UniqueMeshVector streamOuts;  // May become obsolete
    UniqueMesh layerBoost;
    bool isValid = false;

    // For instanced rendering
    std::unordered_map<RoomGeometry, RoomComponentMeshes> uniqueRoomMeshes;
    std::unordered_map<RoomGeometry, std::vector<glm::mat4>> roomInstanceTransforms;

    LayerMeshes() = default;
    DEFAULT_MOVES_DELETE_COPIES(LayerMeshes);
    ~LayerMeshes() = default;

    void render(const int thisLayer, const int focusedLayer, OpenGL &gl);
    explicit operator bool() const { return isValid; }
};

// This must be ordered so we can iterate over the layers from lowest to highest.
using BatchedMeshes = std::map<int, LayerMeshes>;

struct NODISCARD MapBatches final
{
    BatchedMeshes batchedMeshes;
    BatchedConnectionMeshes connectionMeshes;
    BatchedRoomNames roomNameBatches;

    MapBatches() = default;
    ~MapBatches() = default;
    DEFAULT_MOVES_DELETE_COPIES(MapBatches);
};
