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
                 RoomSignalHandler &in_signaler)
    : m_signaler(in_signaler)
    , m_params(in_p)
    , m_paths(std::move(moved_paths))
    // The dummy parent path for Syncing doesn't represent a specific strategy's hold.
    // Pass an empty weak_ptr as its locker, also avoids calling shared_from_this()
    // (or getSharedPtrFromThis()) during make_shared construction of this Syncing instance,
    // which can be problematic before the object is fully constructed.
    , m_parent(Path::alloc(RoomHandle{}, nullptr, this->m_signaler, std::nullopt)) // Changed to nullptr
{}

void Syncing::virt_receiveRoom(const RoomHandle &in_room, ChangeList &changes)
{
    if (++m_numPaths > m_params.maxPaths) {
        if (!m_paths->empty()) {
            for (auto &path : *m_paths) {
                path->deny(changes);
            }
            m_paths->clear();
            m_parent = nullptr;
        }
    } else {
        auto p = Path::alloc(in_room, this, this->m_signaler, ExitDirEnum::NONE); // Changed to this
        p->setParent(m_parent);
        m_parent->insertChild(p);
        m_paths->push_back(p);
    }
}

// Removed getSharedPtrFromThis implementations

std::shared_ptr<PathList> Syncing::evaluate()
{
    return m_paths;
}

void Syncing::finalizePaths(ChangeList &changes)
{
    if (m_parent != nullptr) {
        m_parent->deny(changes);
        // Path::deny sets m_zombie=true, which should be sufficient for state.
        // m_parent = nullptr; // Optional: explicitly reset if Syncing instance can be reused.
    }
}
