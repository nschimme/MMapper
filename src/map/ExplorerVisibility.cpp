// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "ExplorerVisibility.h"

#include "../global/progresscounter.h"
#include "Change.h"
#include "ChangeList.h"
#include "ChangeTypes.h"
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

ExplorerView ExplorerView::makeActive(std::shared_ptr<const ExplorerVisibility> base,
                                      const Map &map,
                                      const std::optional<RoomId> current,
                                      const DiscoveredExits &discovered)
{
    ExplorerView view;
    view.m_active = true;
    view.m_base = std::move(base);
    view.m_current = current;
    if (!current) {
        return view;
    }
    const RoomHandle room = map.findRoomHandle(*current);
    if (!room.exists()) {
        return view;
    }
    // The current room is visible, so the rooms one step beyond it are frontier.
    for (const ExitDirEnum dir : ALL_EXITS_NESWUD) {
        if (!isEffectiveExit(map, room, dir, discovered)) {
            continue;
        }
        for (const RoomId targetId : room.getExit(dir).getOutgoingSet()) {
            const RoomHandle target = map.findRoomHandle(targetId);
            if (target.exists() && !target.isTemporary()) {
                view.m_currentFrontier.insert(targetId);
            }
        }
    }
    return view;
}

bool ExplorerView::shows(const RoomId id) const
{
    if (!m_active) {
        return true;
    }
    return m_current == id || (m_base && m_base->visible.contains(id));
}

bool ExplorerView::isFrontier(const RoomId id) const
{
    if (!m_active || shows(id)) {
        return false;
    }
    return m_currentFrontier.contains(id) || (m_base && m_base->frontier.contains(id));
}

Map hideUndiscoveredExits(const Map &map, const DiscoveredExits &discovered)
{
    // Decide everything against the original map first: whether an exit counts as found can
    // depend on the other side of the same door.
    ChangeList changes;
    map.getRooms().for_each([&](const RoomId id) {
        const RoomHandle room = map.getRoomHandle(id);
        for (const ExitDirEnum dir : ALL_EXITS_NESWUD) {
            const RawExit &exit = room.getExit(dir);
            if (exit.exitIsExit() && exit.doorIsHidden()
                && !isEffectiveExit(map, room, dir, discovered)) {
                changes.add(Change{exit_change_types::NukeExit{id, dir, WaysEnum::OneWay}});
            }
        }
    });
    if (changes.empty()) {
        return map;
    }
    ProgressCounter pc;
    return map.apply(pc, changes).map;
}

Map mapForExplorer(const Map &map, const std::shared_ptr<const DiscoveredExits> &discovered)
{
    return discovered ? hideUndiscoveredExits(map, *discovered) : map;
}
