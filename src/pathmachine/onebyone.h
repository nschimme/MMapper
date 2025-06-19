#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "../map/parseevent.h"
#include "experimenting.h"
// No longer directly needed: #include "pathmachine.h"

#include <memory>

class ParseEvent;
class Path;
class RoomSignalHandler;
struct PathParameters;

// Experimenting is already a PathMachine::PathProcessor
class NODISCARD OneByOne final : public Experimenting
{
private:
    SharedParseEvent event;
    RoomSignalHandler *handler = nullptr;

public:
    explicit OneByOne(const SigParseEvent &sigParseEvent,
                      PathParameters &in_params,
                      RoomSignalHandler *handler);

public: // Changed from private
    void processRoom(const RoomHandle& room) override; // New method

public:
    void addPath(std::shared_ptr<Path> path);
};
