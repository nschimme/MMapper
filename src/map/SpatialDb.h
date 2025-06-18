#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2021 The MMapper Authors

#include "../global/macros.h"
#include "coordinate.h"
#include "roomid.h"

#include <immer/map.hpp>
#include <optional>
#include <ostream>

class AnsiOstream;
class ProgressCounter;

struct NODISCARD SpatialDb final
{
private:
    /// Value is the last room assigned to the coordinate.
    immer::map<Coordinate, RoomId> m_unique;

private:
    // m_bounds and m_needsBoundsUpdate are mutable local state, not part of the persistent data.
    // They can be marked mutable if necessary, or recomputed on demand.
    // For immer, typically such derived data is not stored directly in the immutable object
    // or is handled externally. Let's assume they are recomputed or handled as needed.
    // For now, keep them but be mindful they represent transient state.
    mutable std::optional<Bounds> m_bounds;
    mutable bool m_needsBoundsUpdate = true;

public:
    NODISCARD bool needsBoundsUpdate() const
    {
        return m_needsBoundsUpdate || !m_bounds.has_value();
    }
    NODISCARD const std::optional<Bounds> &getBounds() const {
        // Potentially trigger updateBounds if needed and not const, or ensure it's called appropriately.
        return m_bounds;
    }

public:
    NODISCARD const RoomId *findUnique(const Coordinate &key) const {
        // immer::map::find returns a pointer to the value if found, or nullptr.
        return m_unique.find(key);
    }

public:
    // Modification methods now update m_unique by creating a new map.
    void remove(RoomId /*id*/, const Coordinate &coord) {
        // Note: Original 'id' parameter was unused in OrderedMap::erase based on key.
        // immer::map::erase returns a new map.
        m_unique = m_unique.erase(coord);
        m_needsBoundsUpdate = true;
    }

    void add(RoomId id, const Coordinate &coord) {
        // immer::map::set returns a new map.
        m_unique = m_unique.set(coord, id);
        m_needsBoundsUpdate = true;
    }

    void move(RoomId id, const Coordinate &from, const Coordinate &to) {
        // This involves a remove and an add. Can be done more efficiently if 'from' must exist.
        // For immer, this would be: new_map = old_map.erase(from).set(to, id)
        // Checking if 'from' key's value is indeed 'id' might be an assertion here.
        if (const RoomId* current_id_at_from = m_unique.find(from); current_id_at_from && *current_id_at_from == id) {
             m_unique = m_unique.erase(from).set(to, id);
        } else {
            // Handle error or inconsistency: room 'id' not at 'from', or 'from' doesn't exist.
            // For now, mimic potential original behavior of just setting 'to'.
            // Or, strictly, if 'move' implies 'from' must contain 'id':
            // throw std::runtime_error("Move operation inconsistency");
            // A safer default might be to just add to the new coordinate if the old one isn't as expected.
            m_unique = m_unique.set(to, id);
        }
        m_needsBoundsUpdate = true;
    }

    // updateBounds and printStats operate on the current state.
    // updateBounds modifies mutable members m_bounds and m_needsBoundsUpdate.
    void updateBounds(ProgressCounter &pc) const; // Make const if it only updates mutable members
    void printStats(ProgressCounter &pc, AnsiOstream &os) const;

public:
    // Callback = void(const Coordinate&, RoomId);
    template<typename Callback>
    void for_each(Callback &&callback) const
    {
        static_assert(std::is_invocable_r_v<void, Callback, const Coordinate &, RoomId>);
        // immer::map iterates over std::pair<const Key, Value>
        for (const auto &kv_pair : m_unique) {
            callback(kv_pair.first, kv_pair.second);
        }
    }
    NODISCARD auto size() const { return m_unique.size(); } // size_t usually

public:
    // immer::map has its own operator==
    NODISCARD bool operator==(const SpatialDb &rhs) const { return m_unique == rhs.m_unique; }
    NODISCARD bool operator!=(const SpatialDb &rhs) const { return !(rhs == *this); }
};
