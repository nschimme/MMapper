// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors
#include "SpatialDb.h"

#include "../global/AnsiOstream.h"
#include "../global/progresscounter.h"

#include <ostream>

NODISCARD static bool mightBeOnBoundary(const Coordinate &coord, const Bounds &bounds)
{
#define CHECK(_axis) ((bounds.min._axis) == (coord._axis) || (bounds.max._axis) == (coord._axis))
    return CHECK(x) || CHECK(y) || CHECK(z);
#undef CHECK
}

// findUnique, remove, add, move are now implemented in the header using immer::map's API.
// Their implementations here are redundant and refer to old APIs.

void SpatialDb::updateBounds(ProgressCounter &pc) const
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
