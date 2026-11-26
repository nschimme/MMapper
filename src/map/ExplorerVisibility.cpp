// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "ExplorerVisibility.h"

#include "RawExit.h"
#include "RoomHandle.h"

namespace {
NODISCARD bool isFound(const RoomHandle &room, const ExitDirEnum dir, const DiscoveredExits &found)
{
    const ServerRoomId id = room.getServerId();
    return id != INVALID_SERVER_ROOMID && found.contains(DiscoveredExit{id, dir});
}
} // namespace

bool isEffectiveExit(const Map &map,
                     const RoomHandle &room,
                     const ExitDirEnum dir,
                     const DiscoveredExits &discovered)
{
    const RawExit &exit = room.getExit(dir);
    if (!exit.exitIsExit()) {
        return false;
    }
    if (!exit.doorIsHidden()) {
        return true;
    }
    if (isFound(room, dir, discovered)) {
        return true;
    }

    // The other side of the same secret door: a found hidden exit that leads back here.
    const ExitDirEnum back = opposite(dir);
    for (const RoomId targetId : exit.getOutgoingSet()) {
        const RoomHandle target = map.findRoomHandle(targetId);
        if (!target.exists()) {
            continue;
        }
        const RawExit &reverse = target.getExit(back);
        if (reverse.getOutgoingSet().contains(room.getId()) && isFound(target, back, discovered)) {
            return true;
        }
    }
    return false;
}

ExplorerVisibility computeExplorerVisibility(const ExplorerVisibilityInput &in)
{
    const Map &map = in.map;
    ExplorerVisibility result;

    map.getRooms().for_each([&](const RoomId id) {
        const RoomHandle room = map.getRoomHandle(id);
        if (in.currentRoom == id) {
            result.visible.insert(id);
            return;
        }
        // Temporary rooms are guesses made while mapping; they never prove discovery.
        if (room.isTemporary()) {
            return;
        }
        const ServerRoomId serverId = room.getServerId();
        const bool visible = (serverId == INVALID_SERVER_ROOMID) ? in.placedRooms.contains(id)
                                                                 : in.known.contains(serverId);
        if (visible) {
            result.visible.insert(id);
        }
    });

    // Frontier uses outgoing effective exits of visible rooms only: an exit from an unknown
    // room into a known one says nothing about the unknown room.
    for (const RoomId id : result.visible) {
        const RoomHandle room = map.getRoomHandle(id);
        for (const ExitDirEnum dir : ALL_EXITS_NESWUD) {
            if (!isEffectiveExit(map, room, dir, in.discoveredExits)) {
                continue;
            }
            for (const RoomId targetId : room.getExit(dir).getOutgoingSet()) {
                if (result.visible.contains(targetId)) {
                    continue;
                }
                const RoomHandle target = map.findRoomHandle(targetId);
                if (target.exists() && !target.isTemporary()) {
                    result.frontier.insert(targetId);
                }
            }
        }
    }
    return result;
}
