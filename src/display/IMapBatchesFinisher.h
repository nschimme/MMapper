#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2021 The MMapper Authors

#include "../global/macros.h"

#include <future>
#include <memory>
#include <optional> // For std::optional
#include "../map/roomid.h" // For RoomIdSet (often provides RoomId, maybe RoomIdSet too)
#include "../map/RoomIdSet.h" // Explicitly include for RoomIdSet definition

class GLFont;
class OpenGL;
struct MapBatches;

struct NODISCARD IMapBatchesFinisher
{
public:
    virtual ~IMapBatchesFinisher();

private:
    virtual void virt_finish(MapBatches &output, OpenGL &gl, GLFont &font) const = 0;
    NODISCARD virtual std::optional<RoomIdSet> virt_getProcessedRoomIds() const {
        // Default implementation returns nullopt, derived classes should override if they have this info.
        return std::nullopt;
    }

public:
    void finish(MapBatches &output, OpenGL &gl, GLFont &font) const
    {
        virt_finish(output, gl, font);
    }
    NODISCARD std::optional<RoomIdSet> getProcessedRoomIds() const {
        return virt_getProcessedRoomIds();
    }
};

struct NODISCARD SharedMapBatchFinisher final : public std::shared_ptr<const IMapBatchesFinisher>
{};
using FutureSharedMapBatchFinisher = std::future<SharedMapBatchFinisher>;
