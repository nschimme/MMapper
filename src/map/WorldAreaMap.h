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

    NODISCARD bool operator==(const AreaInfo &other) const {
        return roomSet == other.roomSet; // Relies on mm::CopyOnWrite<RoomIdSet>::operator==
    }
    void remove(RoomId id) {
        // This method is non-const, getMutable is fine.
        if (roomSet.get()->contains(id)) { // Check before getting mutable to avoid needless copy
             roomSet.getMutable()->erase(id);
        }
    }
};

// Note: RoomArea{} is not the same as the global area.
// RoomArea{} contains rooms that do not specify an area,
// while the global area contains all rooms.
struct NODISCARD AreaInfoMap final
{
private:
    using Map = std::map<RoomArea, AreaInfo>;
    Map m_map; // Stores AreaInfo, which now has COW RoomIdSet
    AreaInfo m_global; // Also has COW RoomIdSet

public:
    NODISCARD explicit AreaInfoMap() = default; // Default constructor is fine

public:
    NODISCARD bool contains(const RoomArea &area) const { return find(area) != nullptr; } // find will be updated

    NODISCARD AreaInfo *find(const std::optional<RoomArea> &area) {
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
    NODISCARD const AreaInfo *find(const std::optional<RoomArea> &area) const {
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

    NODISCARD AreaInfo &get(const std::optional<RoomArea> &area) {
        if (area.has_value()) {
            // operator[] will default-construct AreaInfo if area.value() is not found.
            // AreaInfo's default constructor will default-construct its mm::CopyOnWrite<RoomIdSet> member,
            // which in turn creates a new std::shared_ptr<RoomIdSet>(new RoomIdSet()).
            return m_map[area.value()];
        } else {
            return m_global;
        }
    }
    NODISCARD const AreaInfo &get(const std::optional<RoomArea> &area) const {
        if (area.has_value()) {
            auto it = m_map.find(area.value());
            if (it == m_map.end()) {
                // Behavior for const get when area not found should be defined.
                // Throwing is one option, returning a static const default AreaInfo is another.
                // Matching std::map::at behavior by throwing.
                throw std::out_of_range("AreaInfoMap::get() const: area not found");
            }
            return it->second;
        } else {
            return m_global;
        }
    }

    NODISCARD bool operator==(const AreaInfoMap &other) const {
        return m_global == other.m_global && m_map == other.m_map; // Relies on AreaInfo::operator== and std::map::operator==
    }
    NODISCARD size_t numAreas() const { return m_map.size(); }

    NODISCARD auto begin() const { return m_map.begin(); } // std::map iterators are fine
    NODISCARD auto end() const { return m_map.end(); } // std::map iterators are fine

public:
    void insert(const RoomArea &areaName, RoomId id) {
        // This method is non-const, getMutable is fine.
        m_global.roomSet.getMutable()->insert(id); // Modify global set
        if (!areaName.empty()) { // Assuming empty areaName is not stored in m_map explicitly
            AreaInfo& specific_area_info = m_map[areaName]; // Get or create AreaInfo for specific area
            // specific_area_info.roomSet is mm::CopyOnWrite<RoomIdSet>
            specific_area_info.roomSet.getMutable()->insert(id); // Modify specific area's set
        }
    }
    void remove(const RoomArea &areaName, RoomId id) {
        // This method is non-const, getMutable is fine.
        // Check global set before getting mutable to avoid needless copy
        if (m_global.roomSet.get()->contains(id)) {
            m_global.roomSet.getMutable()->erase(id); // Modify global set
        }

        if (!areaName.empty()) {
            auto it = m_map.find(areaName);
            if (it != m_map.end()) {
                AreaInfo& specific_area_info = it->second;
                // Check specific area's set before getting mutable
                if (specific_area_info.roomSet.get()->contains(id)) {
                    specific_area_info.roomSet.getMutable()->erase(id);
                    if (specific_area_info.roomSet.get()->empty()) { // Check if set is empty after erase
                        m_map.erase(it); // Remove area from map if its set is empty
                    }
                }
            }
        }
    }
};
