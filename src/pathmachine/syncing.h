#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "../global/RuleOf5.h"
#include "../map/RoomIdSet.h" // Added RoomIdSet.h
#include "path.h"

#include <list>
#include <memory>

#include <QtGlobal>

class RoomSignalHandler;
struct PathParameters;

class NODISCARD Syncing // Removed final and RoomRecipient inheritance
{
private:
    RoomSignalHandler *signaler = nullptr;
    PathParameters &params;
    const std::shared_ptr<PathList> paths;
    // This is not our parent; it's the parent we assign to new objects.
    std::shared_ptr<Path> parent;
    uint32_t numPaths = 0u;
    RoomIdSet m_collectedRoomIds; // Added

public:
    explicit Syncing(PathParameters &p,
                     std::shared_ptr<PathList> paths,
                     RoomSignalHandler *signaler);

public:
    Syncing() = delete;
    DELETE_CTORS_AND_ASSIGN_OPS(Syncing);
    ~Syncing(); // Removed final

    // receiveRoom methods will be modified/added in .cpp to not be virtual overrides
    // virt_receiveRoom removed as it was part of RoomRecipient interface

public:
    std::shared_ptr<PathList> evaluate();
};
