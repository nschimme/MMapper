#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/ImmUnorderedSet.h"
#include "../global/macros.h"
#include "ChangeList.h"
#include "DiscoveredExit.h"
#include "ExitDirection.h"
#include "Map.h"
#include "RoomIdSet.h"
#include "roomid.h"

#include <memory>
#include <optional>

// What an Explore-mode player is allowed to see of the map. Pure functions of plain values:
// nothing here knows about GMCP, the observer, or rendering, so every consumer (canvas mask,
// labels, dialogs, path finding) can share one definition and tests need no UI.
//
// Everything not listed as visible or frontier is hidden, so a room nobody thought of fails
// closed by being absent.

// A room is visible when its character knows it, or when it is not hidden: reachable from the
// base map's seed rooms (the rooms "Export Base Map" keeps) through exits that are not secret,
// or whose secret the character has found. Only what is behind an undiscovered secret stays
// hidden.
struct NODISCARD ExplorerVisibilityInput final
{
    Map map;
    // Server IDs the character knows (history, list chunks, live adds).
    ImmUnorderedSet<ServerRoomId> known;
    DiscoveredExits discoveredExits;
    // The player's current room: always visible, even when it is temporary or unknown.
    std::optional<RoomId> currentRoom;
    // Rooms without a server ID that the path machine placed the player in this session. Such
    // rooms have no stable key, so they are only ever visible for the session.
    RoomIdSet placedRooms;
};

struct NODISCARD ExplorerVisibility final
{
    // False when the map holds none of the base map's seed rooms (a user's partial map). Then
    // only known rooms, the current room and placed rooms are visible, as before.
    bool baseMapFound = false;
    RoomIdSet visible;
    // The part of `visible` that is shown only because it is not hidden: on the base map, but
    // not discovered (not known and not placed). Never holds the current room or a temporary
    // room. Fog may dim these rooms, but never hides them.
    RoomIdSet undiscoveredBase;
    // Not visible, but one step beyond a visible room through an effective exit.
    RoomIdSet frontier;
};

// A hidden exit counts as no exit until discovered. It is discovered when its own side was
// found, or when the room it leads to has a found exit leading back (the other side of the
// same secret door). A hidden exit on a room without a server ID can only be opened from the
// other side.
NODISCARD extern bool isEffectiveExit(const Map &map,
                                      const RoomHandle &room,
                                      ExitDirEnum dir,
                                      const DiscoveredExits &discovered);

NODISCARD extern ExplorerVisibility computeExplorerVisibility(const ExplorerVisibilityInput &in);

// The map as an Explore-mode player may see it: every hidden exit that has not been discovered
// is removed (flags, door name and connections, on that side only), so everything that reads
// the map (walls, connections, flows, door names) sees a plain wall without having to know
// about secret doors, and a consumer cannot forget one. Returns the same map when there is
// nothing to hide.
NODISCARD extern Map hideUndiscoveredExits(const Map &map, const DiscoveredExits &discovered);

// Whether replacing the finds `before` with `after` can only show more secret exits, never hide
// one. Null means not exploring. Showing more is safe to apply in the background while the
// current meshes keep drawing, since a stale mesh only hides more. Anything that could hide
// something (starting to explore, another character, a set that lost entries) is not: the
// meshes must be dropped at once, or the stale ones would still show the secrets.
NODISCARD extern bool exploreExitsOnlyReveal(const std::shared_ptr<const DiscoveredExits> &before,
                                             const std::shared_ptr<const DiscoveredExits> &after);

// Whether a room may be meshed in a connection. `visible` is the base visibility of Explore mode
// (the service's, which never depends on where the player stands). Null means not exploring:
// nothing is hidden. An empty set means exploring with nothing computed yet: everything is.
NODISCARD extern bool exploreRoomShown(const RoomIdSet *visible, RoomId room);

// A connection (and its door name) is meshed only when every room it touches is shown. A
// connection is not hidden by the fog mask, which keeps empty cells shown so that long lines
// are not cut, so a line between two hidden rooms would otherwise show where they are.
NODISCARD extern bool exploreConnectionShown(const RoomIdSet *visible, RoomId from, RoomId to);

// Whether replacing the base visibility `before` with `after` can only show more. Same contract
// as exploreExitsOnlyReveal(): a mesh built from a smaller set only hides more, so it may keep
// drawing while a new one is built; anything else must drop the meshes at once.
NODISCARD extern bool exploreVisibleOnlyGrows(const std::shared_ptr<const RoomIdSet> &before,
                                              const std::shared_ptr<const RoomIdSet> &after);

// Explore mode is read-only: of what the path machine wants to change, only the backfill of a
// room's server ID is applied (room_change_types::SetServerId); every other change is dropped.
// An empty result means there is nothing to apply.
NODISCARD extern ChangeList onlyServerIdBackfill(const ChangeList &changes);

// The map to mesh: inactive (no snapshot of finds) is the map unchanged.
NODISCARD extern Map mapForExplorer(const Map &map,
                                    const std::shared_ptr<const DiscoveredExits> &discovered);

// What consumers query. The expensive part (`base`, computed without a current room) depends
// only on slowly changing inputs, so it is computed off the GUI thread and shared immutably.
// The player's current room changes on every step, so it is a tiny overlay on top: stepping
// never needs a recompute. A default-constructed view is inactive and shows everything.
class NODISCARD ExplorerView final
{
private:
    bool m_active = false;
    std::shared_ptr<const ExplorerVisibility> m_base;
    std::optional<RoomId> m_current;
    RoomIdSet m_currentFrontier;

public:
    ExplorerView() = default;

    // `base` may be null before the first computation: everything but the current room is
    // then hidden (fail closed).
    NODISCARD static ExplorerView makeActive(std::shared_ptr<const ExplorerVisibility> base,
                                             const Map &map,
                                             std::optional<RoomId> current,
                                             const DiscoveredExits &discovered);

    NODISCARD bool isActive() const { return m_active; }
    // Fully shown. True for everything when inactive.
    NODISCARD bool shows(RoomId id) const;
    // Shown only because it is on the base map, not because it is discovered. Never true for
    // the current room or when inactive.
    NODISCARD bool isUndiscoveredBase(RoomId id) const;
    // Hinted only: one step beyond a shown room. Never true when inactive.
    NODISCARD bool isFrontier(RoomId id) const;
    // Not shown at all. Never true when inactive.
    NODISCARD bool isHidden(RoomId id) const { return m_active && !shows(id) && !isFrontier(id); }
};

namespace test {
extern void testExplorerVisibility();
} // namespace test
