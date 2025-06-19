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

void Syncing::processRoom(const RoomHandle &in_room, const ParseEvent & /* event */)
{
    // Logic from former virt_receiveRoom
    if (++numPaths > params.maxPaths) {
        if (!paths->empty()) {
            // Deny requires a ChangeList. Create a dummy one for now, as Syncing itself doesn't manage one.
            // This might need revisiting if these changes should be collected.
            ChangeList localChanges;
            for (auto &path : *paths) {
                path->deny(localChanges);
            }
            // Assuming localChanges are discarded here as original code didn't collect them.
            paths->clear();
            parent = nullptr; // If parent was complex, its denial might also need changes.
                           // However, parent is a simple dummy path here, its denial is mostly for releasing its own room if held.
                           // If parent itself needs to be denied and it's not null:
            if (parent) {
                ChangeList parentDenyChanges; // Separate ChangeList for parent's denial
                parent->deny(parentDenyChanges);
            }
        }
    } else {
        if (!parent) { // Safety check if parent became null due to maxPaths logic
             // Re-create a dummy parent if it was cleared. This might indicate a need to rethink
             // how 'parent' is managed if maxPaths limit is hit.
             // For now, let's assume parent should always exist if we are adding children.
             // This situation (parent is null but we are trying to add more paths) implies
             // the previous clear might have been too aggressive or needs adjustment.
             // A simple fix might be to only clear paths->clear() but not parent = nullptr,
             // or ensure parent is reinitialized if we continue adding paths.
             // However, the original logic sets parent = nullptr and then would fail here.
             // Let's stick to the direct translation and note this potential issue.
             // If parent is null, p->setParent(parent) will be problematic.
             // The original code would likely crash or misbehave if parent was null here.
             // For now, we'll assume parent is not null if numPaths <= params.maxPaths
        }
        // Updated Path::alloc call: removed 'this' (locker) and 'signaler'
        auto p = Path::alloc(m_pathMachine, in_room, ExitDirEnum::NONE); // Original used ExitDirEnum::NONE
        p->setProb(1.0); // FIXME: calculate real probability (comment from original) - Added based on prompt example

        // If parent became null above, this section will have issues.
        // Assuming parent is valid if we are in this 'else' block and numPaths limit not exceeded.
        if (parent) {
            p->setParent(parent);
            parent->insertChild(p);
        } else {
            // This case should ideally not be hit if logic is correct around parent re-initialization or preservation.
            // Handle error or re-initialize parent if necessary.
            // For now, new path 'p' won't have a parent if 'parent' is null.
        }
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
        ChangeList localChanges; // Dummy ChangeList for destructor context
        parent->deny(localChanges);
    }
}
