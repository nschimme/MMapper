// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors
#include "SpatialDb.h"

#include "../global/AnsiOstream.h"
#include "../global/progresscounter.h"

#include <ostream>
#include <vector> // Required for immer::map::items()

NODISCARD static bool mightBeOnBoundary(const Coordinate &coord, const Bounds &bounds)
{
#define CHECK(_axis) ((bounds.min._axis) == (coord._axis) || (bounds.max._axis) == (coord._axis))
    return CHECK(x) || CHECK(y) || CHECK(z);
#undef CHECK
}

// findUnique is defined in the header and correctly uses m_unique.find(key)
// const RoomId *SpatialDb::findUnique(const Coordinate &key) const
// {
//     return m_unique.find(key);
// }

void SpatialDb::remove(const RoomId /*id*/, const Coordinate &coord)
{
    m_unique = m_unique.erase(coord); // COW update

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
    m_unique = m_unique.set(coord, id); // COW update
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

    // immer::map is unordered. Get all items to initialize bounds and iterate.
    std::vector<std::pair<Coordinate, RoomId>> items = m_unique.items();

    // Initialize bounds with the first element's coordinate
    // (Order doesn't matter for calculating overall bounds)
    const auto &c = items[0].first;
    m_bounds.emplace(c, c);

    pc.increaseTotalStepsBy(items.size());
    for (const auto &kv : items) {
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
