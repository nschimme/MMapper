// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "syncing.h"

#include "../map/ExitDirection.h"
#include "path.h"
#include "pathmachine.h" // Ensure PathMachine and PathProcessor are defined
#include "pathparameters.h"
#include "../map/ChangeList.h" // For dummy ChangeList in destructor

#include <memory>

Syncing::Syncing(PathMachine &pathMachine,
                 PathParameters &in_p,
                 std::shared_ptr<PathList> moved_paths)
    : m_pathMachine(pathMachine) // Initialize m_pathMachine
    , params(in_p)
    , paths(std::move(moved_paths))
    // Updated Path::alloc call: removed 'this' (locker) and 'signaler'
    , parent(Path::alloc(m_pathMachine, RoomHandle{}, std::nullopt))
{}

void Syncing::processRoom(const RoomHandle &in_room) // Removed ParseEvent event
{
    // Simplified: just create and add the path.
    // Pruning logic moved to evaluate().
    // The 'parent' path is a dummy root for new paths created in Syncing mode.
    auto p = Path::alloc(m_pathMachine, in_room, ExitDirEnum::NONE); // Original used ExitDirEnum::NONE
    p->setProb(1.0); // FIXME: calculate real probability (comment from original)

    if (parent) { // Should generally be true unless Path::alloc for parent failed in constructor
        p->setParent(parent);
        parent->insertChild(p);
    }
    // Even if parent is somehow null, we still add the path to the list.
    // PathMachine::evaluatePaths will handle paths with null parents if necessary.
    paths->push_back(p);
    numPaths++; // Keep track of paths added
}

std::shared_ptr<PathList> Syncing::evaluate(ChangeList &changes) // Added ChangeList& changes
{
    if (numPaths > params.maxPaths) {
        // Max paths exceeded, deny all current paths and the dummy parent.
        if (!paths->empty()) {
            for (auto &path : *paths) {
                path->deny(changes); // Use the passed-in ChangeList
            }
            paths->clear();
        }
        if (parent) {
            parent->deny(changes); // Use the passed-in ChangeList
            // We set parent to null here to prevent it from being denied again in the destructor,
            // if Syncing object itself is destroyed shortly after this evaluate call.
            // Denying a path multiple times is generally safe but might add redundant changes.
            parent = nullptr;
        }
        numPaths = 0; // Reset count as all paths are cleared
    }
    // TODO: The original Syncing::evaluate might have had sorting or other logic.
    // For now, it just returns the (potentially pruned) list of paths.
    // If further pruning or selection logic is needed (e.g. keeping best N paths
    // instead of denying all when over limit), it would go here.
    // The current logic (denying all if > maxPaths) matches the old processRoom behavior.
    return paths;
}

Syncing::~Syncing()
{
    // The 'parent' path is a dummy path created and managed by Syncing.
    // If it hasn't been denied and cleared by evaluate() (e.g. if maxPaths wasn't exceeded,
    // or evaluate wasn't called after the last processRoom), it might still exist.
    // However, its denial should produce map changes only if it acquired a real room
    // and was involved in map operations, which a dummy parent usually isn't.
    // If its denial is necessary for ChangeList integrity, that should happen in evaluate().
    // If it's just about releasing a hold on a dummy RoomId=0, that doesn't affect ChangeList.
    // Removing the deny from destructor simplifies ChangeList management here.
    // If 'parent' held a real room that needs release, it should be handled explicitly
    // by PathMachine or by ensuring evaluate() is always called.
    // if (parent != nullptr) {
    //     ChangeList localChanges; // Problematic in destructor if changes are expected to be processed.
    //     parent->deny(localChanges);
    // }
}
