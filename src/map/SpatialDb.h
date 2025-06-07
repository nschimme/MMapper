#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2021 The MMapper Authors

#include "../global/OrderedMap.h"
#include "../global/macros.h"
#include "RoomIdSet.h" // Added for RoomIdSet
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
    OrderedMap<Coordinate, RoomId> m_unique;

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
    NODISCARD const RoomId *findUnique(const Coordinate &key) const;

public:
    void remove(RoomId id, const Coordinate &coord);
    void add(RoomId id, const Coordinate &coord);
    void move(RoomId id, const Coordinate &from, const Coordinate &to);
    void updateBounds(ProgressCounter &pc);
    void printStats(ProgressCounter &pc, AnsiOstream &os) const;

public:
    // Callback = void(const Coordinate&, RoomId);
    template<typename Callback>
    void for_each(Callback &&callback) const
    {
        static_assert(std::is_invocable_r_v<void, Callback, const Coordinate &, RoomId>);
        for (const auto &kv : m_unique) {
            callback(kv.first, kv.second);
        }
    }
    NODISCARD auto size() const { return m_unique.size(); }

    NODISCARD RoomIdSet getRoomIdsInBounds(const Bounds& bounds, int zLevel) const {
        RoomIdSet result;
        if (!m_bounds.has_value()) { // If overall DB bounds don't exist, no rooms exist
            return result;
        }
        // Optimization: check if requested bounds intersects with DB's overall bounds
        // This requires Bounds to have an intersects() method, or implement logic here.
        // For now, iterate all if the zLevel is within the DB's overall Z range.
        if (zLevel < m_bounds->min.z || zLevel > m_bounds->max.z) {
            return result;
        }

        for (const auto &kv : m_unique) {
            const Coordinate& coord = kv.first;
            if (coord.z == zLevel &&
                coord.x >= bounds.min.x && coord.x <= bounds.max.x &&
                coord.y >= bounds.min.y && coord.y <= bounds.max.y) {
                result.insert(kv.second);
            }
        }
        return result;
    }

public:
    NODISCARD bool operator==(const SpatialDb &rhs) const { return m_unique == rhs.m_unique; }
    NODISCARD bool operator!=(const SpatialDb &rhs) const { return !(rhs == *this); }
};
