#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2021 The MMapper Authors

#include "../global/macros.h"

#include <future>
#include <memory>

class GLFont;
class OpenGL;
struct MapBatches;

struct NODISCARD IMapBatchesFinisher
{
public:
    virtual ~IMapBatchesFinisher();

#include <optional> // Required for std::optional
#include <set>      // Required for RoomIdSet (assuming RoomIdSet is std::set<RoomId>)
#include "../map/roomid.h" // For RoomId, assuming RoomIdSet is based on this.

private:
    // virtual void virt_finish(MapBatches &output, OpenGL &gl, GLFont &font) const = 0; // Old const version
    virtual void virt_finish(MapBatches &output_target_on_canvas, OpenGL &gl, GLFont &font, const std::optional<RoomIdSet>& roomsJustUpdated) = 0; // Non-const

public:
    // finish method also becomes non-const as it calls a non-const virt_finish
    void finish(MapBatches &output, OpenGL &gl, GLFont &font, const std::optional<RoomIdSet>& roomsJustUpdated)
    {
        virt_finish(output, gl, font, roomsJustUpdated);
    }
};

// Changed to non-const IMapBatchesFinisher
struct NODISCARD SharedMapBatchFinisher final : public std::shared_ptr<IMapBatchesFinisher>
{};
using FutureSharedMapBatchFinisher = std::future<SharedMapBatchFinisher>;
