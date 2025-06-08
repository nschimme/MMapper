#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2021 The MMapper Authors

#include "../global/OrderedMap.h"
#include "../global/macros.h"
#include "coordinate.h"
#include "roomid.h"
#include "Quadtree.h" // Added include
#include <map>        // Added include
#include <vector>     // Added for std::vector in getRoomsInViewport

#include <optional>
#include <ostream>

class AnsiOstream;
class ProgressCounter;

struct NODISCARD SpatialDb final
{
private:
    /// Value is the last room assigned to the coordinate.
    OrderedMap<Coordinate, RoomId> m_unique;
    std::map<int, Quadtree> m_layerQuadtrees; // Made non-mutable

private:
    std::optional<Bounds> m_bounds;
    bool m_needsBoundsUpdate = true;

public:
    std::vector<RoomId> getRoomsInViewport(int layer, float viewportMinX, float viewportMinY, float viewportMaxX, float viewportMaxY) const; // Added public method
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
    void buildAllQuadtrees(ProgressCounter& pc); // Made non-const, added ProgressCounter
    void clearAllQuadtrees();
    NODISCARD std::optional<RoomBounds> getLayerQuadtreeBounds(int layerId) const;

private:
   void ensureQuadtreeForLayer(int layerId); // Made non-const
   void addToQuadtree(RoomId roomId, const Coordinate& coord);
   void removeFromQuadtree(RoomId roomId, const Coordinate& coord);

public:
    SpatialDb() = default; // Added default constructor
    SpatialDb(const SpatialDb&) = delete; // Deleted copy constructor
    SpatialDb& operator=(const SpatialDb&) = delete; // Deleted copy assignment
    SpatialDb(SpatialDb&&) = default; // Added default move constructor
    SpatialDb& operator=(SpatialDb&&) = default; // Added default move assignment

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

public:
    NODISCARD bool operator==(const SpatialDb &rhs) const { return m_unique == rhs.m_unique; }
    NODISCARD bool operator!=(const SpatialDb &rhs) const { return !(rhs == *this); }
};
