// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2025 The MMapper Authors

#include "WorldAreaMap.h"

NODISCARD bool AreaInfo::operator==(const AreaInfo &other) const
{
    return roomSet == other.roomSet;
}

// AreaInfo::remove is now in the header.

AreaInfoMap::AreaInfoMap() : m_map{}, m_global{} // Initialize with default/empty
{
    // Ensure the "unassigned" area exists.
    m_map = m_map.set(RoomArea{}, AreaInfo{});
    // assert(contains(RoomArea{})); // contains uses find, which is fine.
}

// Non-const find is removed from the header.

const AreaInfo *AreaInfoMap::find(const std::optional<RoomArea> &area) const
{
    if (!area.has_value()) {
        return &m_global;
    }
    // immer::map::find returns const V*
    return m_map.find(area.value());
}

// Non-const get is removed from the header.

const AreaInfo &AreaInfoMap::get(const std::optional<RoomArea> &area) const
{
    const AreaInfo *pArea = find(area);
    if (pArea) {
        return *pArea;
    }
    throw std::runtime_error("invalid map area");
}

bool AreaInfoMap::operator==(const AreaInfoMap &other) const
{
    // Relies on immer::map::operator== and AreaInfo::operator==
    return m_map == other.m_map && m_global == other.m_global;
}

void AreaInfoMap::insert(const RoomArea &areaName, const RoomId id)
{
    // Update global set
    AreaInfo updated_global_info = m_global; // Copy
    updated_global_info.roomSet.insert(id); // Modifies in-place
    m_global = updated_global_info; // Reassign m_global

    // Update specific area in the map
    const AreaInfo* current_area_info_ptr = m_map.find(areaName);
    AreaInfo area_info_to_update; // Default if not found
    if (current_area_info_ptr) {
        area_info_to_update = *current_area_info_ptr; // Copy
    }

    AreaInfo next_area_info = area_info_to_update; // Copy
    next_area_info.roomSet.insert(id); // Modifies in-place
    m_map = m_map.set(areaName, next_area_info); // Update the map
}

void AreaInfoMap::remove(const RoomArea &areaName, const RoomId id)
{
    // Update global set
    AreaInfo updated_global_info = m_global; // Copy
    updated_global_info.remove(id); // Uses AreaInfo::remove which reassigns its roomSet
    m_global = updated_global_info; // Reassign m_global

    // Update specific area in the map
    const AreaInfo* current_area_info_ptr = m_map.find(areaName);
    if (current_area_info_ptr) {
        AreaInfo current_area_info = *current_area_info_ptr; // Copy
        AreaInfo next_area_info = current_area_info; // Copy
        next_area_info.remove(id); // Uses AreaInfo::remove

        // Only update the map if the area info actually changed.
        // This also implicitly handles not creating an area if it didn't exist and we "removed" from it.
        if (next_area_info.roomSet != current_area_info.roomSet) {
             m_map = m_map.set(areaName, next_area_info);
        }
        // Consider: if next_area_info.roomSet is empty, should we remove 'areaName' from m_map?
        // Original logic did not, so this preserves that.
    }
}
