#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2021 The MMapper Authors

#include "../global/macros.h"

#include <future>
#include <memory>
#include <vector>     // For std::vector
#include <utility>    // For std::pair
#include <optional>   // For std::optional

// #include "src/mapdata/remesh_types.h" // Old include, will be removed
#include "src/display/mapcanvas.h"    // For OpenDiablo2::Display::IterativeRemeshMetadata
// Forward declare RoomAreaHash if it's not included via MapBatches.h or mapcanvas.h's includes.
// For now, assuming MapBatches.h (included next) or mapcanvas.h provides it.
// If not, #include "src/display/MapBatches.h" for RoomAreaHash might be needed here
// or ensure remesh_types.h correctly brings it into scope.

class GLFont;
class OpenGL;
struct MapBatches; // Forward declaration is fine here if full definition is in a .cpp or only used by pointer/reference in header.
                  // However, FinishedRemeshPayload uses it by value, so MapBatches.h needs to be included.
#include "src/display/MapBatches.h" // For MapBatches definition and RoomAreaHash

namespace OpenDiablo2 { // Assuming these structs should be within a namespace
namespace Display {

// Payload for finished remesh operation, potentially part of an iterative process.
struct FinishedRemeshPayload {
    MapBatches generatedMeshes; // Requires full definition of MapBatches
    std::vector<std::pair<int, RoomAreaHash>> chunksCompletedThisPass; // RoomAreaHash from MapBatches.h
    bool morePassesNeeded;
    std::optional<OpenDiablo2::Display::IterativeRemeshMetadata> nextIterativeState; // Updated namespace
};

struct NODISCARD IMapBatchesFinisher
{
public:
    virtual ~IMapBatchesFinisher();

private:
    virtual FinishedRemeshPayload virt_finish(OpenGL &gl, GLFont &font) const = 0;

public:
    FinishedRemeshPayload finish(OpenGL &gl, GLFont &font) const
    {
        return virt_finish(gl, font);
    }
};

} // namespace Display
} // namespace OpenDiablo2

struct NODISCARD SharedMapBatchFinisher final : public std::shared_ptr<const OpenDiablo2::Display::IMapBatchesFinisher>
{};
using FutureSharedMapBatchFinisher = std::future<SharedMapBatchFinisher>;
