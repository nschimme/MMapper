#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2021 The MMapper Authors

#include <immer/map.hpp>
#include "../global/macros.h"
#include "coordinate.h"
#include "roomid.h"

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
    std::optional<Bounds> m_bounds;
    bool m_needsBoundsUpdate = true;

public:
    NODISCARD bool needsBoundsUpdate() const
    {
        return m_needsBoundsUpdate || !m_bounds.has_value();
    }
    NODISCARD const std::optional<Bounds> &getBounds() const { return m_bounds; }

public:
    NODISCARD const RoomId *findUnique(const Coordinate &key) const
    {
        return m_unique.find(key);
    }

public:
    // RoomId id parameter in remove was unused with OrderedMap if it's just key-based.
    // Assuming it's for consistency or future use, but immer::map::erase is key-based.
    void remove(RoomId /*id*/, const Coordinate &coord) { m_unique = m_unique.erase(coord); }
    void add(RoomId id, const Coordinate &coord) { m_unique = m_unique.set(coord, id); }
    void move(RoomId id, const Coordinate &from, const Coordinate &to)
    {
        // This needs to be an atomic operation for consistency if 'id' at 'from' is important.
        // If 'id' is just the value to be moved, then simple erase and set is fine.
        // Assuming id is the value being moved.
        m_unique = m_unique.erase(from).set(to, id);
    }
    void updateBounds(ProgressCounter &pc); // Implementation in .cpp might need changes due to m_unique change
    void printStats(ProgressCounter &pc, AnsiOstream &os) const; // dito

public:
    // Callback = void(const Coordinate&, RoomId);
    template<typename Callback>
    void for_each(Callback &&callback) const
    {
        // immer::map's for_each passes const Key&, const Value&.
        // The static_assert should still hold.
        static_assert(std::is_invocable_r_v<void, Callback, const Coordinate &, RoomId>);
        m_unique.for_each(callback);
    }
    NODISCARD auto size() const { return m_unique.size(); }

public:
    NODISCARD bool operator==(const SpatialDb &rhs) const { return m_unique == rhs.m_unique; }
    NODISCARD bool operator!=(const SpatialDb &rhs) const { return !(rhs == *this); }
};
