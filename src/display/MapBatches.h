#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2021 The MMapper Authors

#include "ChunkMeshes.h"

struct NODISCARD MapBatches final
{
    BatchedChunks chunks;
    std::set<int> allLayers;

    MapBatches() = default;
    ~MapBatches() = default;
    DEFAULT_MOVES_DELETE_COPIES(MapBatches);
};
