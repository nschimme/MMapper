#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2025 The MMapper Authors

#include "../global/CopyOnWrite.h" // Added
#include "../global/macros.h"
#include "RoomIdSet.h"
#include "mmapper2room.h"

#include <map>
#include <stdexcept> // For std::out_of_range

class ProgressCounter;

struct NODISCARD AreaInfo final
{
    mm::CopyOnWrite<RoomIdSet> roomSet;

    NODISCARD bool operator==(const AreaInfo &other) const;
    void remove(RoomId id);
};

// Note: RoomArea{} is not the same as the global area.
// RoomArea{} contains rooms that do not specify an area,
// while the global area contains all rooms.
struct NODISCARD AreaInfoMap final
{
private:
    using Map = std::map<RoomArea, AreaInfo>;
    Map m_map;         // Stores AreaInfo, which now has COW RoomIdSet
    AreaInfo m_global; // Also has COW RoomIdSet

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
    void insert(const RoomArea &areaName, RoomId id);
    void remove(const RoomArea &areaName, RoomId id);
};
