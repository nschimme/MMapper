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
    // A base-map room that is not discovered, once fog dims it instead of showing it. The
    // room-cell shader fogs every value below SHOWN, so nothing may write this value until it
    // draws the level; see the `undiscoveredTexel` parameter below.
    static constexpr uint8_t UNDISCOVERED = 192;

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
//
// `undiscoveredTexel` is what a base-map room that is not discovered gets, which is how the fog
// level reaches the shader. SHOWN (the default) means no fog on the base map. Secrets stay
// HIDDEN, and the current room is always SHOWN, at every level.
NODISCARD extern ExplorerFogMask buildExplorerFogMask(
    const Map &map, const ExplorerView &view, uint8_t undiscoveredTexel = ExplorerFogMask::SHOWN);

namespace test {
extern void testExplorerFogMask();
} // namespace test
