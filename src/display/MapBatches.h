#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2021 The MMapper Authors

#include "Connections.h"
#include "MapCanvasData.h"

#include <map>

template<typename T>
using RoomTintArray = EnumIndexedArray<T, RoomTintEnum, NUM_ROOM_TINTS>;

struct NODISCARD LayerMeshes final
{
    UniqueMeshVector batch_less_blended_transparent;
    UniqueMeshVector batch_lequal_blended_transparent;
    UniqueMeshVector batch_equal_blended_transparent;
    RoomTintArray<UniqueMesh> tints;
    UniqueMesh layerBoost;
    bool isValid = false;

    LayerMeshes() = default;
    DEFAULT_MOVES_DELETE_COPIES(LayerMeshes);
    ~LayerMeshes() = default;

    void render(int thisLayer, int focusedLayer);
    explicit operator bool() const { return isValid; }
};

using PlainQuadBatch = std::vector<glm::vec3>;

struct NODISCARD LayerMeshesIntermediate final
{
    using Fn = std::function<UniqueMesh(OpenGL &)>;
    using FnVec = std::vector<Fn>;
    FnVec batch_less_blended_transparent;
    FnVec batch_lequal_blended_transparent;
    FnVec batch_equal_blended_transparent;
    RoomTintArray<PlainQuadBatch> tints;
    PlainQuadBatch layerBoost;
    bool isValid = false;

    LayerMeshesIntermediate() = default;
    DEFAULT_MOVES_DELETE_COPIES(LayerMeshesIntermediate);
    ~LayerMeshesIntermediate() = default;

    NODISCARD LayerMeshes getLayerMeshes(OpenGL &gl) const;
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
