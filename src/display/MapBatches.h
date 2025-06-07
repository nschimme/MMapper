#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2021 The MMapper Authors

// ChunkId type definition has been removed as it's no longer the primary key for batches.
// #include <cstdint> // Was for ChunkId if it were a specific int type.

#include <array> // For std::array
#include "../map/mmapper2room.h" // For RoomArea, NUM_ROOM_TINTS and TintIndices
#include "Connections.h"
#include "MapCanvasData.h"

#include <map>

// Restoring RoomTintArray using std::array and NUM_ROOM_TINTS
template<typename T>
using RoomTintArray = std::array<T, NUM_ROOM_TINTS>;

struct NODISCARD LayerMeshes final
{
    RoomArea m_area; // Added RoomArea identifier
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

    // Constructor to initialize m_area, defaulting to an empty RoomArea
    explicit LayerMeshes(RoomArea area = RoomArea{})
        : m_area(std::move(area)), isValid(false) {} // Ensure isValid is also initialized
    DEFAULT_MOVES_DELETE_COPIES(LayerMeshes);
    ~LayerMeshes() = default;

    void render(int thisLayer, int focusedLayer);
    explicit operator bool() const { return isValid; }
};

// This must be ordered so we can iterate over the layers from lowest to highest.
// RoomArea is now the primary key for meshes within a layer.
using AreaLayerMeshes = std::map<RoomArea, LayerMeshes>;

struct NODISCARD MapBatches final
{
    std::map<int, AreaLayerMeshes> batchedMeshes;
    std::map<int, std::map<RoomArea, ConnectionMeshes>> connectionMeshes;
    std::map<int, std::map<RoomArea, UniqueMesh>> roomNameBatches;

    MapBatches() = default;
    ~MapBatches() = default;
    DEFAULT_MOVES_DELETE_COPIES(MapBatches);

    // Method to merge another MapBatches into this one
    void merge(MapBatches&& other) {
        for (auto& layer_pair : other.batchedMeshes) {
            for (auto& area_pair : layer_pair.second) {
                batchedMeshes[layer_pair.first][area_pair.first] = std::move(area_pair.second);
            }
        }
        other.batchedMeshes.clear();

        for (auto& layer_pair : other.connectionMeshes) {
            for (auto& area_pair : layer_pair.second) {
                connectionMeshes[layer_pair.first][area_pair.first] = std::move(area_pair.second);
            }
        }
        other.connectionMeshes.clear();

        for (auto& layer_pair : other.roomNameBatches) {
            for (auto& area_pair : layer_pair.second) {
                roomNameBatches[layer_pair.first][area_pair.first] = std::move(area_pair.second);
            }
        }
        other.roomNameBatches.clear();
    }
};
