#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2025 The MMapper Authors

#include "../global/macros.h"
#include "RoomIdSet.h" // Will be replaced by immer::set
#include "mmapper2room.h"
#include "roomid.h" // For RoomId

#include <immer/map.hpp>
#include <immer/set.hpp>

#include <map> // For std::map's iterators if needed, or can be removed if fully replaced

class ProgressCounter;

struct NODISCARD AreaInfo final
{
    using ImmerRoomIdSet = immer::set<RoomId>;
    ImmerRoomIdSet roomSet;

    NODISCARD bool operator==(const AreaInfo &other) const { return roomSet == other.roomSet; }
    // void remove(RoomId id); // This method will need to be updated in .cpp
    // For COW, remove would be: roomSet = roomSet.erase(id);
};

// Note: RoomArea{} is not the same as the global area.
// RoomArea{} contains rooms that do not specify an area,
// while the global area contains all rooms.
struct NODISCARD AreaInfoMap final
{
private:
    using Map = immer::map<RoomArea, AreaInfo>;
    Map m_map;
    AreaInfo m_global; // This AreaInfo will now use immer::set internally

public:
    NODISCARD explicit AreaInfoMap();

public:
    NODISCARD bool contains(const RoomArea &area) const { return find(area) != nullptr; }

    NODISCARD AreaInfo *find(const std::optional<RoomArea> &area);
    NODISCARD const AreaInfo *find(const std::optional<RoomArea> &area) const;

    NODISCARD AreaInfo &get(const std::optional<RoomArea> &area);
    NODISCARD const AreaInfo &get(const std::optional<RoomArea> &area) const;

    NODISCARD bool operator==(const AreaInfoMap &other) const
    {
        return m_map == other.m_map && m_global == other.m_global;
    }
    NODISCARD size_t numAreas() const { return m_map.size(); }

    // Iteration for immer::map is different, typically via for_each or by converting to std::vector
    // For now, removing begin()/end() as direct replacement is not straightforward
    // and depends on usage. If direct iteration is needed, it will be handled in a later step
    // when .cpp files are updated.
    // NODISCARD auto begin() const { return m_map.begin(); }
    // NODISCARD auto end() const { return m_map.end(); }

public:
    // These methods will need updating in .cpp for COW semantics
    // void insert(const RoomArea &areaName, RoomId id);
    // void remove(const RoomArea &areaName, RoomId id);
};
