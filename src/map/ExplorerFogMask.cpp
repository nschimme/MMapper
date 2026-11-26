// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "ExplorerFogMask.h"

#include "RoomHandle.h"

#include <algorithm>

ExplorerFogMask buildExplorerFogMask(const Map &map, const ExplorerView &view)
{
    ExplorerFogMask mask;
    if (!view.isActive()) {
        return mask;
    }
    const auto bounds = map.getBounds();
    if (!bounds) {
        return mask;
    }
    mask.min = bounds->min;
    mask.width = bounds->max.x - bounds->min.x + 1;
    mask.height = bounds->max.y - bounds->min.y + 1;
    mask.depth = bounds->max.z - bounds->min.z + 1;
    mask.texels.assign(static_cast<size_t>(mask.width) * static_cast<size_t>(mask.height)
                           * static_cast<size_t>(mask.depth),
                       ExplorerFogMask::SHOWN);

    map.getRooms().for_each([&](const RoomId id) {
        const Coordinate pos = map.getRoomHandle(id).getPosition();
        if (!mask.contains(pos)) {
            return;
        }
        // Anything not explicitly shown or hinted at stays hidden.
        mask.texels[mask.indexOf(pos)] = view.shows(id)        ? ExplorerFogMask::SHOWN
                                         : view.isFrontier(id) ? ExplorerFogMask::FRONTIER
                                                               : ExplorerFogMask::HIDDEN;
    });
    return mask;
}
