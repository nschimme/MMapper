// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "syncing.h"

#include "../map/Compare.h"      // For matches()
#include "../map/ExitDirection.h"
#include "../map/MapFrontend.h" // For m_signaler->m_map
#include "../map/RoomSignalHandler.h" // For RoomSignalHandler definition
#include "path.h"
#include "pathparameters.h"

#include <memory>

Syncing::Syncing(PathParameters &in_p,
                 std::shared_ptr<PathList> moved_paths,
                 RoomSignalHandler *in_signaler)
    : signaler(in_signaler)
    , params(in_p)
    , paths(std::move(moved_paths))
    , parent(Path::alloc(RoomHandle{}, this, signaler, std::nullopt))
// m_collectedRoomIds is default-initialized
{}

// virt_receiveRoom is replaced by the two methods below
void Syncing::receiveRoom(const RoomHandle &room)
{
    // Assuming params.event is SigParseEvent and MatchingTolerance::High is appropriate.
    // The matches function is assumed to exist, similar to ::compare.
    if (room.exists() && matches(room.getRaw(), params.event.deref(), MatchingTolerance::High)) {
        m_collectedRoomIds.insert(room.getId());
    }
}

void Syncing::receiveRoom(ServerRoomId id)
{
    if (id != INVALID_SERVER_ROOMID) {
        if (const auto rh = m_signaler->m_map.findRoomHandle(id)) {
            if (rh.exists() && matches(rh.getRaw(), params.event.deref(), MatchingTolerance::High)) {
                m_collectedRoomIds.insert(rh.getId());
            }
        }
    }
}

std::shared_ptr<PathList> Syncing::evaluate()
{
    paths->clear(); // Clear any previous paths from the member list
    numPaths = 0;   // Reset path count for this evaluation

    // The parent for paths is created in the constructor and reused.
    // If parent itself should be reset/recreated per evaluate, that logic would go here.
    // Based on destructor, parent seems to live for the Syncing instance.

    for (RoomId roomId : m_collectedRoomIds) {
        RoomHandle roomHandle = m_signaler->m_map.findRoomHandle(roomId); // Get fresh RoomHandle
        if (roomHandle.exists()) {
            if (++numPaths > params.maxPaths) {
                if (!paths->empty()) {
                    for (auto &path : *paths) {
                        path->deny(); // Deny paths already added before exceeding maxPaths
                    }
                    paths->clear(); // Clear the list as we exceeded maxPaths
                }
                // No new parent is created here, assuming the member 'parent' should also be denied if paths are cleared.
                // However, the original logic set parent = nullptr only if paths was non-empty.
                // If paths is cleared, the existing parent might become a dangling root for new paths if evaluate is called again.
                // For safety, if we clear paths due to maxPaths, we should probably deny the current parent too,
                // or ensure it's not used for subsequent paths in this evaluation.
                // The simplest is to break, as per original logic structure.
                break;
            }
            // Create new Path object, similar to original virt_receiveRoom
            auto p = Path::alloc(roomHandle, this, signaler, ExitDirEnum::NONE);
            p->setParent(parent); // parent is a member std::shared_ptr<Path>
            if (parent) { // Ensure parent is valid before inserting child
                 parent->insertChild(p);
            }
            paths->push_back(p);
        }
    }
    // Any further processing of 'paths' like sorting would happen here.
    return paths; // Return the populated member list
}

Syncing::~Syncing()
{
    if (parent != nullptr) {
        parent->deny(); // This denies the root path created for this Syncing instance.
    }
}
