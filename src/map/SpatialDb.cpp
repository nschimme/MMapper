// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors
#include "SpatialDb.h"

#include "../global/AnsiOstream.h"
#include "../global/progresscounter.h"
#include "Quadtree.h" // Required for QtRect, Quadtree

#include <ostream>
#include <set> // Required for std::set in buildAllQuadtrees
#include <utility> // Required for std::piecewise_construct, std::forward_as_tuple

NODISCARD static bool mightBeOnBoundary(const Coordinate &coord, const Bounds &bounds)
{
#define CHECK(_axis) ((bounds.min._axis) == (coord._axis) || (bounds.max._axis) == (coord._axis))
    return CHECK(x) || CHECK(y) || CHECK(z);
#undef CHECK
}

const RoomId *SpatialDb::findUnique(const Coordinate &key) const
{
    return m_unique.find(key);
}

void SpatialDb::remove(const RoomId id, const Coordinate &coord)
{
    m_unique.erase(coord);
    removeFromQuadtree(id, coord); // Added call

    if (!m_bounds || mightBeOnBoundary(coord, *m_bounds)) {
        m_needsBoundsUpdate = true;
    }
}

void SpatialDb::add(const RoomId id, const Coordinate &coord)
{
    if (!m_bounds) {
        m_bounds.emplace(coord, coord);
    } else {
        m_bounds->insert(coord);
    }
    m_unique.set(coord, id);
    addToQuadtree(id, coord); // Added call
}

void SpatialDb::move(const RoomId id, const Coordinate &from, const Coordinate &to)
{
    if (from == to) {
        return;
    }

    remove(id, from);
    add(id, to);
}

void SpatialDb::updateBounds(ProgressCounter &pc)
{
    m_bounds.reset();
    m_needsBoundsUpdate = false;
    if (m_unique.empty()) {
        return;
    }

    const auto &c = m_unique.begin()->first;
    m_bounds.emplace(c, c);
    pc.increaseTotalStepsBy(m_unique.size());
    for (const auto &kv : m_unique) {
        m_bounds->insert(kv.first);
        pc.step();
    }
}

void SpatialDb::printStats(ProgressCounter & /*pc*/, AnsiOstream &os) const
{
    if (m_bounds.has_value()) {
        const auto &bounds = m_bounds.value();
        const Coordinate &max = bounds.max;
        const Coordinate &min = bounds.min;

        static constexpr auto green = getRawAnsi(AnsiColor16Enum::green);

        auto show = [&os](std::string_view prefix, int lo, int hi) {
            os << prefix << ColoredValue(green, hi - lo + 1) << " (" << ColoredValue(green, lo)
               << " to " << ColoredValue(green, hi) << ").\n";
        };

        os << "\n";
        show("Width:  ", min.x, max.x);
        show("Height: ", min.y, max.y);
        show("Layers: ", min.z, max.z);
    }
}

// ensureQuadtreeForLayer now only creates if not present, does not populate from m_unique.
// Population is handled by buildAllQuadtrees or addToQuadtree.
void SpatialDb::ensureQuadtreeForLayer(int layerId) {
    if (m_layerQuadtrees.find(layerId) == m_layerQuadtrees.end()) {
        float minX = -10000, minY = -10000, width = 20000, height = 20000; // Large default
        if (m_bounds) {
            minX = m_bounds->min.x;
            minY = m_bounds->min.y;
            width = m_bounds->max.x - m_bounds->min.x + 1;
            height = m_bounds->max.y - m_bounds->min.y + 1;
        }
        if (width <=0) width = 20000;
        if (height <=0) height = 20000;

        m_layerQuadtrees.emplace(std::piecewise_construct,
                                std::forward_as_tuple(layerId),
                                std::forward_as_tuple(RoomBounds(minX, minY, width, height)));
    }
}

void SpatialDb::buildAllQuadtrees(ProgressCounter& pc) {
    m_layerQuadtrees.clear();
    std::map<int, std::vector<RoomEntry>> layerItems;
    if (m_unique.empty()) return;

    // Consolidate bounds calculation and item gathering
    float overallMinX = m_unique.begin()->first.x, overallMinY = m_unique.begin()->first.y;
    float overallMaxX = overallMinX, overallMaxY = overallMaxY;

    pc.setNewTask(ProgressMsg{"Gathering items for Quadtrees"}, m_unique.size());
    for(const auto& pair : m_unique) {
        const Coordinate& coord = pair.first;
        layerItems[coord.z].emplace_back(RoomEntry{pair.second, RoomBounds(coord.x, coord.y, 1.0f, 1.0f)});
        if (coord.x < overallMinX) overallMinX = coord.x;
        if (coord.y < overallMinY) overallMinY = coord.y;
        if (coord.x > overallMaxX) overallMaxX = coord.x;
        if (coord.y > overallMaxY) overallMaxY = coord.y;
        pc.step();
    }

    // Fallback for width/height if only one point exists
    float overallWidth = (overallMaxX - overallMinX) + 1.0f;
    float overallHeight = (overallMaxY - overallMinY) + 1.0f;
    if (overallWidth <= 0) overallWidth = 1.0f; // Default small width
    if (overallHeight <= 0) overallHeight = 1.0f; // Default small height


    pc.setNewTask(ProgressMsg{"Building Quadtrees per layer"}, layerItems.size());
    for(auto const& [layerId, items] : layerItems) {
        // Use overall bounds for each layer's quadtree for simplicity,
        // or calculate per-layer bounds if preferred (more complex here).
        RoomBounds layerBounds(overallMinX, overallMinY, overallWidth, overallHeight);

        // Max items per node and max levels could be constants or configurable
        m_layerQuadtrees.emplace(std::piecewise_construct,
                                std::forward_as_tuple(layerId),
                                std::forward_as_tuple(layerBounds, 4, 8));
        Quadtree& currentTree = m_layerQuadtrees.at(layerId);
        for(const auto& item : items) {
            currentTree.insert(item.id, item.bounds);
        }
        pc.step();
    }
}

void SpatialDb::clearAllQuadtrees() {
    m_layerQuadtrees.clear();
}

std::vector<RoomId> SpatialDb::getRoomsInViewport(int layer, float viewportMinX, float viewportMinY, float viewportMaxX, float viewportMaxY) const {
    auto it = m_layerQuadtrees.find(layer);
    if (it == m_layerQuadtrees.end()) {
        return {}; // Quadtree for this layer doesn't exist (e.g., layer is empty or buildAllQuadtrees not called)
    }
    // Ensure viewport width/height are positive for RoomBounds
    float vpWidth = viewportMaxX - viewportMinX;
    float vpHeight = viewportMaxY - viewportMinY;
    if (vpWidth < 0) vpWidth = 0;
    if (vpHeight < 0) vpHeight = 0;
    RoomBounds viewportRect(viewportMinX, viewportMinY, vpWidth, vpHeight);
    return it->second.queryRange(viewportRect);
}

void SpatialDb::addToQuadtree(RoomId roomId, const Coordinate& coord) {
    ensureQuadtreeForLayer(coord.z); // Ensures tree exists, using overall bounds if new
    auto it = m_layerQuadtrees.find(coord.z);
    // it should not be end() here due to ensureQuadtreeForLayer
    it->second.insert(roomId, RoomBounds(coord.x, coord.y, 1.0f, 1.0f));
}

void SpatialDb::removeFromQuadtree(RoomId roomId, const Coordinate& coord) {
    auto it = m_layerQuadtrees.find(coord.z);
    if (it != m_layerQuadtrees.end()) {
        it->second.remove(roomId, RoomBounds(coord.x, coord.y, 1.0f, 1.0f));
    }
}
