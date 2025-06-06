#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2021 The MMapper Authors

#include "../global/macros.h"

#include <future>
#include <memory>

class GLFont;
class OpenGL;
struct MapBatches;
// class QOpenGLExtraFunctions; // Forward declaration removed

struct NODISCARD IMapBatchesFinisher
{
public:
    virtual ~IMapBatchesFinisher();

private:
    // Signature reverted
    virtual void virt_finish(MapBatches &output, OpenGL &gl, GLFont &font) const = 0;

public:
    // Signature reverted
    void finish(MapBatches &output, OpenGL &gl, GLFont &font) const
    {
        virt_finish(output, gl, font);
    }
};

struct NODISCARD SharedMapBatchFinisher final : public std::shared_ptr<const IMapBatchesFinisher>
{};
using FutureSharedMapBatchFinisher = std::future<SharedMapBatchFinisher>;
