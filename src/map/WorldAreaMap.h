#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2025 The MMapper Authors

#include "../global/macros.h"
#include "RoomIdSet.h" // Already uses immer::set
#include "mmapper2room.h"

#include <immer/map.hpp>
#include <optional> // Make sure it's included for std::optional

class ProgressCounter;

struct NODISCARD AreaInfo final
{
    RoomIdSet roomSet; // Already BasicRoomIdSet<RoomId> -> immer::set<RoomId>

    NODISCARD bool operator==(const AreaInfo &other) const;
    // remove will change the current AreaInfo object by reassigning its roomSet
    void remove(RoomId id) {
        // BasicRoomIdSet::erase now returns a new BasicRoomIdSet.
        // This method modifies the AreaInfo instance's roomSet.
        if (roomSet.contains(id)) { // Optional: check if needed, erase on non-existent is no-op for m_set
            this->roomSet = this->roomSet.erase(id);
        }
    }
};

// Note: RoomArea{} is not the same as the global area.
// RoomArea{} contains rooms that do not specify an area,
// while the global area contains all rooms.
struct NODISCARD AreaInfoMap final
{
private:
    using ImmerMap = immer::map<RoomArea, AreaInfo>;
    ImmerMap m_map;
    AreaInfo m_global; // Manages its own RoomIdSet immutably

public:
    NODISCARD explicit AreaInfoMap();

public:
    NODISCARD bool contains(const RoomArea &area) const {
        return m_map.count(area) > 0;
    }

    // Non-const find is removed. Modifications go through specific methods on AreaInfoMap.
    NODISCARD const AreaInfo *find(const std::optional<RoomArea> &area) const;

    // Non-const get is removed.
    NODISCARD const AreaInfo &get(const std::optional<RoomArea> &area) const;

    NODISCARD bool operator==(const AreaInfoMap &other) const;
    NODISCARD size_t numAreas() const { return m_map.size(); }

    // Iterators for const access
    NODISCARD auto begin() const { return m_map.begin(); } // Returns immer::map::iterator (which is a const_iterator)
    NODISCARD auto end() const { return m_map.end(); }

public:
    // Modification methods will update m_map or m_global by reassignment
    void insert(const RoomArea &areaName, RoomId id);
    void remove(const RoomArea &areaName, RoomId id);
};
