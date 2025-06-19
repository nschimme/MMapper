#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "../map/parseevent.h"
#include "experimenting.h"

#include <memory>

class ParseEvent;
class Path;
class RoomSignalHandler;
struct PathParameters;

class NODISCARD OneByOne final : public Experimenting
{
private:
    SharedParseEvent event;
    // RoomSignalHandler *handler = nullptr; // Removed member

public:
    explicit OneByOne(const SigParseEvent &sigParseEvent, // Constructor updated
                      PathParameters &in_params);
                      // RoomSignalHandler *handler); // Removed parameter

private:
    void processRoom(const RoomHandle &room); // Renamed and 'final' removed

public:
    void addPath(std::shared_ptr<Path> path);
};
