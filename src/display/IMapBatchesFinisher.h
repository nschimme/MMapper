#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2021 The MMapper Authors

#include "../global/macros.h"
#include "MapBatches.h"

#include <future>
#include <memory>
#include <set>

class GLFont;
class OpenGL;
struct MapBatches;

struct NODISCARD IMapBatchesFinisher
{
public:
    virtual ~IMapBatchesFinisher();

    virtual const std::set<ChunkId> &getDirtyChunks() const = 0;

private:
    virtual void virt_finish(MapBatches &output, OpenGL &gl, GLFont &font) const = 0;

public:
    void finish(MapBatches &output, OpenGL &gl, GLFont &font) const
    {
        virt_finish(output, gl, font);
    }
};

struct NODISCARD SharedMapBatchFinisher final : public std::shared_ptr<const IMapBatchesFinisher>
{};
using FutureSharedMapBatchFinisher = std::future<SharedMapBatchFinisher>;
