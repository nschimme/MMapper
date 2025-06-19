#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "../global/RuleOf5.h"
// #include "../map/RoomRecipient.h" // Removed
#include "path.h"
#include "pathmachine.h" // Added for PathProcessor, PathMachine, ParseEvent

#include <list>
#include <memory>

#include <QtGlobal>

// class RoomSignalHandler; // Removed forward declaration
struct PathParameters;
class ParseEvent; // Forward declaration for ParseEvent

class NODISCARD Syncing final : public PathProcessor // Inherit from PathProcessor
{
private:
    PathMachine &m_pathMachine; // Added member
    // RoomSignalHandler *signaler = nullptr; // Removed member
    PathParameters &params;
    const std::shared_ptr<PathList> paths;
    // This is not our parent; it's the parent we assign to new objects.
    std::shared_ptr<Path> parent; // This member seems unused in the provided virt_receiveRoom logic, but kept for now.
    uint32_t numPaths = 0u;

public:
    explicit Syncing(PathMachine &pathMachine, // Added pathMachine
                     PathParameters &p,
                     std::shared_ptr<PathList> paths);
                     // RoomSignalHandler *signaler); // Removed signaler

public:
    Syncing() = delete;
    DELETE_CTORS_AND_ASSIGN_OPS(Syncing);
    ~Syncing() final;

public: // Added processRoom, removed virt_receiveRoom
    void processRoom(const RoomHandle &room, const ParseEvent &event) override;
    // private:
    // void virt_receiveRoom(const RoomHandle &) final; // Removed

public:
    std::shared_ptr<PathList> evaluate();
};
