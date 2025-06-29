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

NODISCARD bool LocalAreaInfo::operator==(const LocalAreaInfo &other) const
{
    return roomSet == other.roomSet;
}

void LocalAreaInfo::remove(const RoomId id)
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
    m_map.set(RoomArea{}, LocalAreaInfo{});
    assert(contains(RoomArea{}));
}

void AreaInfoMap::init(const std::unordered_map<RoomArea, LocalAreaInfo> &map,
                       const AreaInfo &global)
{
    m_map.init(map);
    m_global = global;
}

NODISCARD bool AreaInfoMap::contains(const RoomArea &area) const
{
    return findNonGlobalArea(area) != nullptr;
}

NODISCARD const LocalAreaInfo *AreaInfoMap::findNonGlobalArea(const RoomArea &area) const
{
    return m_map.find(area);
}

NODISCARD const LocalAreaInfo &AreaInfoMap::getNonGlobalArea(const RoomArea &area) const
{
    if (const LocalAreaInfo *const pArea = findNonGlobalArea(area)) {
        return *pArea;
    }
    throw std::runtime_error("invalid non-global map area");
}

bool AreaInfoMap::operator==(const AreaInfoMap &other) const
{
    return m_map == other.m_map && m_global == other.m_global;
}

void AreaInfoMap::insert(const RoomArea &areaName, const RoomId id)
{
    m_global.roomSet.insert(id); // Global area always gets the room

    // Update the specific non-global area
    m_map.set(areaName, [id, ptr = m_map.find(areaName)]() -> LocalAreaInfo {
        LocalAreaInfo tmp;
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
    if (const LocalAreaInfo *const it = m_map.find(areaName)) {
        const LocalAreaInfo &info = *it;
        if (info.roomSet.contains(id)) {
            // special case: remove the area when the last room is removed
            if (info.roomSet.size() == 1) {
                m_map.erase(areaName);
            } else {
                m_map.update(areaName, [id](LocalAreaInfo &area) { area.remove(id); });
            }
        }
    }
}
