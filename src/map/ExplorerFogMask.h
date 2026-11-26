#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"
#include "ExplorerVisibility.h"
#include "Map.h"
#include "coordinate.h"

#include <cstddef>
#include <cstdint>
#include <vector>

// The Explore-mode fog as data: one byte per map cell, laid out as `depth` layers of
// `width` x `height` texels (row-major, one layer per z) so the renderer can upload it as a
// 2D array texture and shaders can look a cell up from a world position.
//
// Only cells that hold a room are ever fogged. A cell without a room has nothing to hide, and
// must stay shown so that a long connection between two shown rooms is not cut where it passes
// over empty cells. A cell outside the bounds is always hidden (fail closed).
struct NODISCARD ExplorerFogMask final
{
    static constexpr uint8_t HIDDEN = 0;
    static constexpr uint8_t FRONTIER = 128;
    static constexpr uint8_t SHOWN = 255;

    Coordinate min;
    int width = 0;
    int height = 0;
    int depth = 0;
    std::vector<uint8_t> texels;

    NODISCARD bool empty() const { return texels.empty(); }
    NODISCARD bool contains(const Coordinate c) const
    {
        return c.x >= min.x && c.x < min.x + width && c.y >= min.y && c.y < min.y + height
               && c.z >= min.z && c.z < min.z + depth;
    }
    NODISCARD size_t indexOf(const Coordinate c) const
    {
        return (static_cast<size_t>(c.z - min.z) * static_cast<size_t>(height)
                + static_cast<size_t>(c.y - min.y))
                   * static_cast<size_t>(width)
               + static_cast<size_t>(c.x - min.x);
    }
    // What the shader sees for the cell containing `c`.
    NODISCARD uint8_t at(const Coordinate c) const
    {
        return contains(c) ? texels[indexOf(c)] : HIDDEN;
    }
};

// An inactive view yields an empty mask: the renderer then does not fog at all.
NODISCARD extern ExplorerFogMask buildExplorerFogMask(const Map &map, const ExplorerView &view);

namespace test {
extern void testExplorerFogMask();
} // namespace test
