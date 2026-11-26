// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "ExplorerSecrets.h"

#include "ExplorerVisibility.h"
#include "RawExit.h"

bool isSecretExit(const RoomHandle &room, const ExitDirEnum dir)
{
    const RawExit &exit = room.getExit(dir);
    return exit.exitIsExit() && exit.doorIsHidden();
}

ExplorerFoundMask buildExplorerFoundMask(const Map &map, const DiscoveredExits &discovered)
{
    ExplorerFoundMask mask;
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
                       0);

    map.getRooms().for_each([&](const RoomId id) {
        const RoomHandle room = map.getRoomHandle(id);
        const Coordinate pos = room.getPosition();
        if (!mask.contains(pos)) {
            return;
        }
        for (const ExitDirEnum dir : ALL_EXITS_NESWUD) {
            // isEffectiveExit() also counts the far side of a door found from the other side.
            if (isSecretExit(room, dir) && isEffectiveExit(map, room, dir, discovered)) {
                mask.texels[mask.indexOf(pos)] |= foundSecretBit(dir);
            }
        }
    });
    return mask;
}

std::vector<FoundSecretTexel> foundSecretTexels(const Map &map, const DiscoveredExit &found)
{
    std::vector<FoundSecretTexel> result;
    const RoomHandle room = map.findRoomHandle(found.room);
    if (!room.exists() || found.dir == ExitDirEnum::UNKNOWN || found.dir == ExitDirEnum::NONE
        || !isSecretExit(room, found.dir)) {
        return result;
    }
    result.push_back(FoundSecretTexel{room.getPosition(), found.dir});

    // The other side of the same door, if it is a secret exit that leads back here.
    const ExitDirEnum back = opposite(found.dir);
    for (const RoomId targetId : room.getExit(found.dir).getOutgoingSet()) {
        const RoomHandle target = map.findRoomHandle(targetId);
        if (target.exists() && isSecretExit(target, back)
            && target.getExit(back).getOutgoingSet().contains(room.getId())) {
            result.push_back(FoundSecretTexel{target.getPosition(), back});
        }
    }
    return result;
}
