// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2025 The MMapper Authors

#include "WorldAreaMap.h"

// operator== was already updated in the header for AreaInfo with immer::set.

// This method was commented out in the header change, but if it were to be
// part of AreaInfo's public API and used externally, it would be:
/*
void AreaInfo::remove(const RoomId id)
{
    // This assumes AreaInfo itself is mutable here, which is not the case
    // if it's obtained from an immer::map.
    // This method should ideally not exist if AreaInfo is always COW.
    // If AreaInfo is a standalone mutable object, this is fine.
    // Given its use in AreaInfoMap, it's treated as COW.
    // roomSet = roomSet.erase(id); // This would be the COW way.
}
*/

AreaInfoMap::AreaInfoMap()
{
    // COW: Use set to initialize the map
    m_map = m_map.set(RoomArea{}, AreaInfo{});
    // contains() will use the new m_map.find()
    assert(contains(RoomArea{}));
}

// Changed to return const AreaInfo* to align with immer::map::find
// and COW principles. Callers should not attempt to modify the pointed-to AreaInfo.
const AreaInfo *AreaInfoMap::find(const std::optional<RoomArea> &area)
{
    if (!area.has_value()) {
        // m_global is a member, so returning its address is fine if m_global
        // itself is treated with COW semantics for its 'roomSet'.
        return &m_global;
    }
    // immer::map::find returns const Value*
    return m_map.find(area.value());
}

const AreaInfo *AreaInfoMap::find(const std::optional<RoomArea> &area) const
{
    if (!area.has_value()) {
        return &m_global;
    }
    return m_map.find(area.value());
}

// Changed to return const AreaInfo&
const AreaInfo &AreaInfoMap::get(const std::optional<RoomArea> &area)
{
    // This now calls the modified find that returns const AreaInfo*
    if (const AreaInfo *const pArea = find(area)) {
        return *pArea;
    }
    throw std::runtime_error("invalid map area");
}
const AreaInfo &AreaInfoMap::get(const std::optional<RoomArea> &area) const
{
    if (const AreaInfo *const pArea = find(area)) {
        return *pArea;
    }
    throw std::runtime_error("invalid map area");
}

// operator== was already updated in the header for AreaInfoMap.

void AreaInfoMap::insert(const RoomArea &areaName, const RoomId id)
{
    // COW update for m_global's roomSet
    AreaInfo updated_global = m_global; // Copy m_global
    updated_global.roomSet = updated_global.roomSet.insert(id); // Modify copy's roomSet
    m_global = updated_global; // Assign copy back

    // Get current AreaInfo for areaName, or a new one if it doesn't exist
    const AreaInfo *current_specific_area_ptr = m_map.find(areaName);
    AreaInfo updated_specific_area = current_specific_area_ptr ? *current_specific_area_ptr : AreaInfo{};

    updated_specific_area.roomSet = updated_specific_area.roomSet.insert(id);
    m_map = m_map.set(areaName, updated_specific_area);
}

void AreaInfoMap::remove(const RoomArea &areaName, const RoomId id)
{
    // COW update for m_global's roomSet
    AreaInfo updated_global = m_global; // Copy m_global
    if (updated_global.roomSet.count(id)) { // Check if id exists before erase
        updated_global.roomSet = updated_global.roomSet.erase(id); // Modify copy's roomSet
        m_global = updated_global; // Assign copy back
    }

    // COW update for specific area in m_map
    if (const AreaInfo *current_specific_area_ptr = m_map.find(areaName)) {
        AreaInfo updated_specific_area = *current_specific_area_ptr; // Copy
        if (updated_specific_area.roomSet.count(id)) { // Check if id exists
            updated_specific_area.roomSet = updated_specific_area.roomSet.erase(id); // Modify copy
            m_map = m_map.set(areaName, updated_specific_area); // Set copy back
        }
        // If roomSet becomes empty, AreaInfo object for areaName is kept.
        // Original logic didn't remove AreaInfo if its roomSet became empty.
    }
}
