// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2025 The MMapper Authors

#include "WorldAreaMap.h"
#include <stdexcept> // For std::out_of_range (used in AreaInfoMap::get const)
#include <optional>  // For std::optional

// --- AreaInfo method definitions ---

NODISCARD bool AreaInfo::operator==(const AreaInfo &other) const {
    return roomSet == other.roomSet; // Relies on mm::CopyOnWrite<RoomIdSet>::operator==
}

void AreaInfo::remove(RoomId id) {
    // This method is non-const, getMutable is fine.
    if (roomSet.get()->contains(id)) { // Check before getting mutable to avoid needless copy
         roomSet.getMutable()->erase(id);
    }
}

// --- AreaInfoMap method definitions ---

AreaInfoMap::AreaInfoMap()
{
    // The previous subtask noted that this explicit insertion for RoomArea{}
    // might not be needed with the inline .h versions of get/insert.
    // However, the instruction is to keep this constructor body.
    // If `contains` (which is inline and uses `find`) has issues with this,
    // it might need adjustment, but for now, following instructions.
    m_map[RoomArea{}] = AreaInfo{};
    assert(contains(RoomArea{}));
}

NODISCARD AreaInfo *AreaInfoMap::find(const std::optional<RoomArea> &area) {
    if (area.has_value()) {
        auto it = m_map.find(area.value());
        if (it == m_map.end()) {
            return nullptr;
        }
        return &it->second;
    } else {
        return &m_global;
    }
}

NODISCARD const AreaInfo *AreaInfoMap::find(const std::optional<RoomArea> &area) const {
    if (area.has_value()) {
        auto it = m_map.find(area.value());
        if (it == m_map.end()) {
            return nullptr;
        }
        return &it->second;
    } else {
        return &m_global;
    }
}

NODISCARD AreaInfo &AreaInfoMap::get(const std::optional<RoomArea> &area) {
    if (area.has_value()) {
        // operator[] will default-construct AreaInfo if area.value() is not found.
        return m_map[area.value()];
    } else {
        return m_global;
    }
}

NODISCARD const AreaInfo &AreaInfoMap::get(const std::optional<RoomArea> &area) const {
    if (area.has_value()) {
        auto it = m_map.find(area.value());
        if (it == m_map.end()) {
            throw std::out_of_range("AreaInfoMap::get() const: area not found");
        }
        return it->second;
    } else {
        return m_global;
    }
}

NODISCARD bool AreaInfoMap::operator==(const AreaInfoMap &other) const {
    return m_global == other.m_global && m_map == other.m_map;
}

void AreaInfoMap::insert(const RoomArea &areaName, RoomId id) {
    m_global.roomSet.getMutable()->insert(id);
    // Get or create the AreaInfo for areaName.
    // std::map::operator[] correctly handles areaName being RoomArea{} (empty string),
    // and will use the AreaInfo object initialized by AreaInfoMap's constructor
    // or create a new default AreaInfo if areaName is a new non-empty key.
    AreaInfo& specific_area_info = m_map[areaName];
    specific_area_info.roomSet.getMutable()->insert(id);
}

void AreaInfoMap::remove(const RoomArea &areaName, RoomId id) {
    if (m_global.roomSet.get()->contains(id)) {
        m_global.roomSet.getMutable()->erase(id);
    }

    if (!areaName.empty()) {
        auto it = m_map.find(areaName);
        if (it != m_map.end()) {
            AreaInfo& specific_area_info = it->second;
            if (specific_area_info.roomSet.get()->contains(id)) {
                specific_area_info.roomSet.getMutable()->erase(id);
                if (specific_area_info.roomSet.get()->empty()) {
                    m_map.erase(it);
                }
            }
        }
    }
}
