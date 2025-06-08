#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2021 The MMapper Authors

#include "../global/macros.h"

#include <future>
#include <memory>
#include <vector>
#include <utility> // For std::pair
#include <optional>
#include <memory> // Added for std::unique_ptr

// #include "mapcanvas.h" // Removed
// Include for MapBatches definition (ensure it's there if not pulled by others)
#include "MapBatches.h"

struct IterativeRemeshMetadata; // Forward declaration

class GLFont;
class OpenGL;
// MapBatches is now included above if necessary, or forward declared if sufficient

// Placed in global namespace as per plan
struct FinishedRemeshPayload {
    MapBatches generatedMeshes;
    std::vector<std::pair<int, RoomAreaHash>> chunksCompletedThisPass;
    bool morePassesNeeded;
    std::optional<std::unique_ptr<IterativeRemeshMetadata>> nextIterativeState; // Changed to unique_ptr
};

struct NODISCARD IMapBatchesFinisher
{
public:
    virtual ~IMapBatchesFinisher();

private:
    // Return type changed to global FinishedRemeshPayload
    virtual FinishedRemeshPayload virt_finish(OpenGL &gl, GLFont &font) const = 0;

public:
    // Return type changed to global FinishedRemeshPayload
    FinishedRemeshPayload finish(OpenGL &gl, GLFont &font) const
    {
        return virt_finish(gl, font);
    }
};

// SharedMapBatchFinisher refers to IMapBatchesFinisher, no change needed in its definition itself due to this.
struct NODISCARD SharedMapBatchFinisher final : public std::shared_ptr<const IMapBatchesFinisher>
{};
using FutureSharedMapBatchFinisher = std::future<SharedMapBatchFinisher>;
