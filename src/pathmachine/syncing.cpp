// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "syncing.h"

#include "../map/ExitDirection.h"
#include "path.h"
#include "pathparameters.h"

#include <memory>

Syncing::Syncing(PathParameters &in_p,
                 std::shared_ptr<PathList> moved_paths,
                 RoomSignalHandler &in_signaler) // Changed to reference
    : m_signaler(in_signaler) // Prefixed
    , m_params(in_p) // Prefixed
    , m_paths(std::move(moved_paths)) // Prefixed
    // The dummy parent path for Syncing doesn't represent a specific strategy's hold.
    // Pass an empty weak_ptr as its locker, also avoids calling shared_from_this()
    // (or getSharedPtrFromThis()) during make_shared construction of this Syncing instance,
    // which can be problematic before the object is fully constructed.
    , m_parent(Path::alloc(RoomHandle{}, std::weak_ptr<PathProcessor>(), this->m_signaler, std::nullopt)) // Prefixed
{}

void Syncing::virt_receiveRoom(const RoomHandle &in_room, ChangeList &changes)
{
    if (++m_numPaths > m_params.maxPaths) { // Prefixed numPaths, params
        if (!m_paths->empty()) { // Prefixed paths
            for (auto &path : *m_paths) { // Prefixed paths
                path->deny(changes); // Pass ChangeList
            }
            m_paths->clear(); // Prefixed paths
            m_parent = nullptr; // Prefixed parent
        }
    } else {
        // Use getSharedPtrFromThis (which returns shared_ptr, convertible to weak_ptr for Path::alloc)
        auto p = Path::alloc(in_room, this->getSharedPtrFromThis(), this->m_signaler, ExitDirEnum::NONE); // Prefixed signaler
        p->setParent(m_parent); // Prefixed parent
        m_parent->insertChild(p); // Prefixed parent
        m_paths->push_back(p); // Prefixed paths
    }
}

std::shared_ptr<PathProcessor> Syncing::getSharedPtrFromThis() {
    return shared_from_this();
}

std::shared_ptr<const PathProcessor> Syncing::getSharedPtrFromThis() const {
    return shared_from_this();
}

std::shared_ptr<PathList> Syncing::evaluate()
{
    return m_paths; // Prefixed
}

void Syncing::finalizePaths(ChangeList &changes)
{
    if (m_parent != nullptr) { // Prefixed
        m_parent->deny(changes); // Prefixed // Pass ChangeList
        // Path::deny sets m_zombie=true, which should be sufficient for state.
        // m_parent = nullptr; // Optional: explicitly reset if Syncing instance can be reused.
    }
}
