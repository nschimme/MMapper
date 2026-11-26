#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"
#include "DiscoveredExit.h"
#include "ExitDirection.h"
#include "Map.h"
#include "RoomHandle.h"
#include "coordinate.h"

#include <cstddef>
#include <cstdint>
#include <vector>

// A secret exit: an exit with a hidden door. Explore mode meshes each one twice, as a wall and
// as a door, and the shader keeps the form that matches whether the character has found it.
NODISCARD extern bool isSecretExit(const RoomHandle &room, ExitDirEnum dir);

// The bit of `dir` in a cell's found-secret byte. Only the six real directions have one.
NODISCARD constexpr uint8_t foundSecretBit(const ExitDirEnum dir)
{
    return static_cast<uint8_t>(1u << static_cast<unsigned>(dir));
}

// Which secret exits the character has found, as data: one byte per map cell, laid out as
// `depth` layers of `width` x `height` texels (row-major, one layer per z) like ExplorerFogMask,
// so it can be uploaded as a 2D array texture beside it. Bit `foundSecretBit(dir)` of a cell is
// set when the secret exit of the room in that cell, in direction `dir`, is found. Both sides of
// a door are set when either side was found. A cell outside the bounds reads 0 (not found).
struct NODISCARD ExplorerFoundMask final
{
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
    NODISCARD uint8_t at(const Coordinate c) const { return contains(c) ? texels[indexOf(c)] : 0; }
    NODISCARD bool isFound(const Coordinate c, const ExitDirEnum dir) const
    {
        return (at(c) & foundSecretBit(dir)) != 0;
    }
};

// The mask for all the finds. Empty when the map has no bounds.
NODISCARD extern ExplorerFoundMask buildExplorerFoundMask(const Map &map,
                                                          const DiscoveredExits &discovered);

struct NODISCARD FoundSecretTexel final
{
    Coordinate cell;
    ExitDirEnum dir = ExitDirEnum::NONE;
};

// The texels one newly found secret exit sets, both sides of its door: what
// buildExplorerFoundMask() would add for it, without touching the rest of the map. Empty if the
// room is unknown or the exit is not a secret.
NODISCARD extern std::vector<FoundSecretTexel> foundSecretTexels(const Map &map,
                                                                 const DiscoveredExit &found);

namespace test {
extern void testExplorerSecrets();
} // namespace test
