#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2021 The MMapper Authors

#include "../global/macros.h"

#include <future>
#include <memory>

class OpenGL;
struct MapBatches;

struct NODISCARD IMapBatchesFinisher
{
public:
    virtual ~IMapBatchesFinisher();

private:
    virtual void virt_finish(MapBatches &output, OpenGL &gl) const = 0;

public:
    void finish(MapBatches &output, OpenGL &gl) const { virt_finish(output, gl); }
};

struct NODISCARD SharedMapBatchFinisher final : public std::shared_ptr<const IMapBatchesFinisher>
{};
using FutureSharedMapBatchFinisher = std::future<SharedMapBatchFinisher>;
