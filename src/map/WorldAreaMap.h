#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2025 The MMapper Authors

#include "../global/macros.h"
#include "RoomIdSet.h"
#include "mmapper2room.h"
// #include "coordinate.h" // Bounds no longer needed here directly
// #include <optional> // std::optional no longer needed here directly

#include <map>
// #include <functional> // std::function no longer needed for remove signature

class ProgressCounter;

struct NODISCARD AreaInfo final
{
    RoomIdSet roomSet;
    // std::optional<Bounds> m_bounds; // REMOVED bounds member

    NODISCARD bool operator==(const AreaInfo &other) const;
    void remove(RoomId id);

    // NODISCARD std::optional<Bounds> getBounds() const { return m_bounds; } // REMOVED getter for bounds
};

// Note: RoomArea{} is not the same as the global area.
// RoomArea{} contains rooms that do not specify an area,
// while the global area contains all rooms.
struct NODISCARD AreaInfoMap final
{
private:
    using Map = std::map<RoomArea, AreaInfo>;
    Map m_map;
    AreaInfo m_global;

public:
    NODISCARD explicit AreaInfoMap();

public:
    NODISCARD bool contains(const RoomArea &area) const { return find(area) != nullptr; }

    NODISCARD AreaInfo *find(const std::optional<RoomArea> &area);
    NODISCARD const AreaInfo *find(const std::optional<RoomArea> &area) const;

    NODISCARD AreaInfo &get(const std::optional<RoomArea> &area);
    NODISCARD const AreaInfo &get(const std::optional<RoomArea> &area) const;

    NODISCARD bool operator==(const AreaInfoMap &other) const;
    NODISCARD size_t numAreas() const { return m_map.size(); }

    NODISCARD auto begin() const { return m_map.begin(); }
    NODISCARD auto end() const { return m_map.end(); }

public:
    // Reverted signature
    void insert(const RoomArea &areaName, RoomId id);
    // Reverted signature
    void remove(const RoomArea &areaName, RoomId id);
};
