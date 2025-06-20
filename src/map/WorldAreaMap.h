#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2025 The MMapper Authors

#include "../global/ImmUnorderedMap.h"
#include "../global/macros.h"
#include "RoomIdSet.h"
#include "mmapper2room.h"

#include <unordered_map>

class ProgressCounter;

struct NODISCARD AreaInfo final
{
    RoomIdSet roomSet;

    NODISCARD bool operator==(const AreaInfo &other) const;
    void remove(RoomId id);
};

// Note: RoomArea{} is not the same as the global area.
// RoomArea{} contains rooms that do not specify an area,
// while the global area contains all rooms.
struct NODISCARD AreaInfoMap final
{
private:
    using Map = ImmUnorderedMap<RoomArea, AreaInfo>;
    Map m_map;
    AreaInfo m_global;

public:
    NODISCARD explicit AreaInfoMap();
    void init(const std::unordered_map<RoomArea, AreaInfo> &map, const AreaInfo &global);

public:
    NODISCARD bool contains(const RoomArea &area) const { return find(area) != nullptr; }

    NODISCARD const AreaInfo *find(const std::optional<RoomArea> &area) const;
    NODISCARD const AreaInfo &get(const std::optional<RoomArea> &area) const;

    NODISCARD bool operator==(const AreaInfoMap &other) const;
    NODISCARD size_t numAreas() const { return m_map.size(); }

    NODISCARD auto begin() const { return m_map.begin(); }
    NODISCARD auto end() const { return m_map.end(); }

public:
    void insert(const RoomArea &areaName, RoomId id);
    void remove(const RoomArea &areaName, RoomId id);
};
