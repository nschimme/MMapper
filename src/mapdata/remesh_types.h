// Copyright 2024 The OpenDiablo2 Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <vector>
#include <set>
#include <utility> // For std::pair
#include <map>     // For RoomAreaHash (placeholder, assuming it might be complex)

#include "src/display/MapBatches.h" // For RoomAreaHash (size_t)

namespace OpenDiablo2 {
namespace MapData {

// Metadata for iterative remeshing process.
struct IterativeRemeshMetadata {
    // All chunks that need to be remeshed in the current iterative process.
    std::vector<std::pair<int, RoomAreaHash>> allTargetChunks;

    // Chunks that have been successfully remeshed in previous passes.
    std::set<std::pair<int, RoomAreaHash>> completedChunks;

    // Current pass number in the iterative remeshing process.
    int currentPassNumber;

    // Strategy for remeshing.
    enum class Strategy {
        AllAtOnce,             // Remesh all chunks in a single pass.
        IterativeViewportPriority // Remesh viewport chunks first, then others iteratively.
    };
    Strategy strategy;

    // Chunks currently in the viewport, prioritized in IterativeViewportPriority strategy.
    std::vector<std::pair<int, RoomAreaHash>> viewportChunks;
};

} // namespace MapData
} // namespace OpenDiablo2
