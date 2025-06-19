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
                 RoomSignalHandler *in_signaler)
    : signaler(in_signaler)
    , params(in_p)
    , paths(std::move(moved_paths))
    // Pass WeakHandle to Path::alloc in constructor
    , parent(Path::alloc(RoomHandle{}, this->getWeakHandleFromThis(), signaler, std::nullopt))
{}

void Syncing::virt_receiveRoom(const RoomHandle &in_room, ChangeList &changes)
{
    if (++numPaths > params.maxPaths) {
        if (!paths->empty()) {
            for (auto &path : *paths) {
                path->deny(changes); // Pass ChangeList
            }
            paths->clear();
            parent = nullptr;
        }
    } else {
        // Pass WeakHandle to Path::alloc in virt_receiveRoom
        auto p = Path::alloc(in_room, this->getWeakHandleFromThis(), signaler, ExitDirEnum::NONE);
        p->setParent(parent);
        parent->insertChild(p);
        paths->push_back(p);
    }
}

std::shared_ptr<PathList> Syncing::evaluate()
{
    return paths;
}

void Syncing::finalizePaths(ChangeList &changes)
{
    if (parent != nullptr) {
        parent->deny(changes); // Pass ChangeList
        // Path::deny sets m_zombie=true, which should be sufficient for state.
        // parent = nullptr; // Optional: explicitly reset if Syncing instance can be reused.
    }
}
