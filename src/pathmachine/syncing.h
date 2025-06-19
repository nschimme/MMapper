#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "../global/RuleOf5.h"
// RoomRecipient.h removed as Syncing no longer inherits from it
#include "path.h"

#include <list>
#include <memory>

#include <QtGlobal>

class RoomSignalHandler;
struct PathParameters;

class NODISCARD Syncing final // Removed inheritance from RoomRecipient
{
private:
    // RoomSignalHandler *signaler = nullptr; // Removed member
    PathParameters &params;
    const std::shared_ptr<PathList> paths;
    // This is not our parent; it's the parent we assign to new objects.
    std::shared_ptr<Path> parent;
    uint32_t numPaths = 0u;

public:
    explicit Syncing(PathParameters &p, // Constructor updated
                     std::shared_ptr<PathList> paths);
                     // RoomSignalHandler *signaler); // Removed parameter

public:
    Syncing() = delete;
    DELETE_CTORS_AND_ASSIGN_OPS(Syncing);
    ~Syncing(); // Removed final

private:
    void processRoom(const RoomHandle &room); // Renamed and final removed

public:
    std::shared_ptr<PathList> evaluate();
};
