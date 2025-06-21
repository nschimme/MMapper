// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "syncing.h"

#include "../map/ExitDirection.h"
#include "path.h"
#include "pathparameters.h"
#include "patheventcontext.h" // Include for mmapper::PathEventContext

#include <memory>

// Syncing::Syncing(PathParameters &in_p, std::shared_ptr<PathList> moved_paths, RoomSignalHandler& in_signaler)
Syncing::Syncing(mmapper::PathEventContext &context, // Added context
                 PathParameters &in_p,
                 std::shared_ptr<PathList> moved_paths,
                 RoomSignalHandler& in_signaler)
    : m_context{context} // Initialize m_context
    , signaler(in_signaler)
    , params(in_p)
    , paths(std::move(moved_paths))
    , parent(Path::alloc(RoomHandle{}, signaler, std::nullopt))
{}

void Syncing::virt_receiveRoom(const RoomHandle &in_room)
{
    if (++numPaths > params.maxPaths) {
        if (!paths->empty()) {
            for (auto &path_ptr : *paths) { // Renamed path to path_ptr
                // path->deny(); becomes:
                deref(path_ptr).deny(m_context);
            }
            paths->clear();
            parent = nullptr; // If parent is nulled, destructor won't call deny on it.
        }
    } else {
        auto p = Path::alloc(in_room, signaler, ExitDirEnum::NONE);
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
        // parent->deny(); becomes:
        parent->deny(m_context);
    }
}
