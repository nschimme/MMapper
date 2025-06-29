#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2025 The MMapper Authors

#include "../global/ImmUnorderedMap.h"
#include "../global/macros.h"
#include "RoomIdSet.h" // Includes ImmRoomIdSet and now ImmUnorderedRoomIdSet
#include "mmapper2room.h"

#include <unordered_map>
#include <variant> // For find/get return types if needed

class ProgressCounter;

struct NODISCARD AreaInfo final
{
    ImmRoomIdSet roomSet; // Ordered set for global area

    NODISCARD bool operator==(const AreaInfo &other) const;
    void remove(RoomId id);
    // Insert is implicitly handled by roomSet.insert()
};

struct NODISCARD NonGlobalAreaInfo final
{
    ImmUnorderedRoomIdSet roomSet; // Unordered set for non-global areas

    NODISCARD bool operator==(const NonGlobalAreaInfo &other) const;
    void remove(RoomId id);
    // Insert is implicitly handled by roomSet.insert()
};

// Note: RoomArea{} is not the same as the global area.
// RoomArea{} contains rooms that do not specify an area,
// while the global area contains all rooms.
struct NODISCARD AreaInfoMap final
{
private:
    using NonGlobalMap = ImmUnorderedMap<RoomArea, NonGlobalAreaInfo>;
    NonGlobalMap m_map; // Stores non-global areas
    AreaInfo m_global;  // Stores the global area info

public:
    NODISCARD explicit AreaInfoMap();
    // Init will need to take NonGlobalAreaInfo for the map part
    void init(const std::unordered_map<RoomArea, NonGlobalAreaInfo> &map,
              const AreaInfo &global);

public:
    NODISCARD bool contains(const RoomArea &area) const; // Checks non-global map

    // Return type might need to be a variant or a wrapper to abstract the difference
    // For now, let's provide separate finders or make users aware.
    // A simpler approach for now: find returns a pointer to the respective type,
    // and get might throw or return a variant/optional.
    NODISCARD const AreaInfo *findGlobalArea() const { return &m_global; }
    NODISCARD const NonGlobalAreaInfo *findNonGlobalArea(const RoomArea &area) const;

    // get will throw if not found or wrong type is requested implicitly
    NODISCARD const AreaInfo &getGlobalArea() const { return m_global; }
    NODISCARD const NonGlobalAreaInfo &getNonGlobalArea(const RoomArea &area) const;

    // A generic find could return a variant
    using FindResultVariant = std::variant<const AreaInfo *, const NonGlobalAreaInfo *, std::nullptr_t>;
    NODISCARD FindResultVariant find(const std::optional<RoomArea> &area) const;

    NODISCARD bool operator==(const AreaInfoMap &other) const;
    NODISCARD size_t numAreas() const { return m_map.size(); } // Number of non-global areas

    // Iterator for non-global areas
    NODISCARD auto begin() const { return m_map.begin(); }
    NODISCARD auto end() const { return m_map.end(); }

public:
    // Insert/remove will need to differentiate between global and non-global updates
    void insert(const RoomArea &areaName, RoomId id);
    void remove(const RoomArea &areaName, RoomId id);
};
