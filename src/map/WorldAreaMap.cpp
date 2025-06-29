// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2025 The MMapper Authors

#include "WorldAreaMap.h"

NODISCARD bool AreaInfo::operator==(const AreaInfo &other) const
{
    return roomSet == other.roomSet;
}

void AreaInfo::remove(const RoomId id)
{
    if (!roomSet.contains(id)) {
        return;
    }
    roomSet.erase(id);
}

NODISCARD bool NonGlobalAreaInfo::operator==(const NonGlobalAreaInfo &other) const
{
    return roomSet == other.roomSet;
}

void NonGlobalAreaInfo::remove(const RoomId id)
{
    if (!roomSet.contains(id)) {
        return;
    }
    roomSet.erase(id);
}

AreaInfoMap::AreaInfoMap()
{
    // Initialize the default non-global area (RoomArea{})
    // m_global is default constructed.
    m_map.set(RoomArea{}, NonGlobalAreaInfo{});
    assert(contains(RoomArea{}));
}

void AreaInfoMap::init(const std::unordered_map<RoomArea, NonGlobalAreaInfo> &map,
                       const AreaInfo &global)
{
    m_map.init(map);
    m_global = global;
}

NODISCARD bool AreaInfoMap::contains(const RoomArea &area) const
{
    return findNonGlobalArea(area) != nullptr;
}

NODISCARD const NonGlobalAreaInfo *AreaInfoMap::findNonGlobalArea(const RoomArea &area) const
{
    return m_map.find(area);
}

NODISCARD const NonGlobalAreaInfo &AreaInfoMap::getNonGlobalArea(const RoomArea &area) const
{
    if (const NonGlobalAreaInfo *const pArea = findNonGlobalArea(area)) {
        return *pArea;
    }
    throw std::runtime_error("invalid non-global map area");
}

NODISCARD AreaInfoMap::FindResultVariant AreaInfoMap::find(
    const std::optional<RoomArea> &area) const
{
    if (!area.has_value()) {
        return &m_global;
    }
    if (const auto it = m_map.find(area.value()); it != nullptr) {
        return it;
    }
    return nullptr;
}

bool AreaInfoMap::operator==(const AreaInfoMap &other) const
{
    return m_map == other.m_map && m_global == other.m_global;
}

void AreaInfoMap::insert(const RoomArea &areaName, const RoomId id)
{
    m_global.roomSet.insert(id); // Global area always gets the room

    // Update the specific non-global area
    m_map.set(areaName, [id, ptr = m_map.find(areaName)]() -> NonGlobalAreaInfo {
        NonGlobalAreaInfo tmp;
        if (ptr != nullptr) {
            tmp = *ptr;
        }
        tmp.roomSet.insert(id);
        return tmp;
    }());
}

void AreaInfoMap::remove(const RoomArea &areaName, const RoomId id)
{
    m_global.remove(id); // Remove from global area

    // Remove from the specific non-global area
    if (const NonGlobalAreaInfo *const it = m_map.find(areaName)) {
        const NonGlobalAreaInfo &info = *it;
        if (info.roomSet.contains(id)) {
            // special case: remove the area when the last room is removed
            if (info.roomSet.size() == 1) {
                m_map.erase(areaName);
            } else {
                m_map.update(areaName, [id](NonGlobalAreaInfo &area) { area.remove(id); });
            }
        }
    }
}
