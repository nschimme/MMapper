// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "syncing.h"

#include "../map/ExitDirection.h"
#include "path.h"
#include "pathparameters.h"

#include <memory>

Syncing::Syncing(PathParameters &in_p, // Constructor updated
                 std::shared_ptr<PathList> moved_paths)
    // RoomSignalHandler *in_signaler) // Removed parameter
    : params(in_p) // signaler(in_signaler) removed
    , paths(std::move(moved_paths))
    // TODO: Replace nullptr for RoomRecipient and RoomSignalHandler if Path::alloc is updated or a proper alternative is found.
    , parent(Path::alloc(RoomHandle{}, static_cast<RoomRecipient*>(nullptr), static_cast<RoomSignalHandler*>(nullptr), std::nullopt))
{}

void Syncing::processRoom(const RoomHandle &in_room) // Renamed method
{
    if (++numPaths > params.maxPaths) {
        if (!paths->empty()) {
            for (auto &path : *paths) {
                path->deny();
            }
            paths->clear();
            parent = nullptr;
        }
    } else {
        // TODO: Replace nullptr for RoomRecipient and RoomSignalHandler if Path::alloc is updated or a proper alternative is found.
        auto p = Path::alloc(in_room, static_cast<RoomRecipient*>(nullptr), static_cast<RoomSignalHandler*>(nullptr), ExitDirEnum::NONE);
        p->setParent(parent);
        parent->insertChild(p);
        paths->push_back(p);
    }
}

std::shared_ptr<PathList> Syncing::evaluate()
{
    return paths;
}

Syncing::~Syncing()
{
    if (parent != nullptr) {
        parent->deny();
    }
}
