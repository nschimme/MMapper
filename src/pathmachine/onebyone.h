#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "../map/parseevent.h"
#include "experimenting.h"

#include <memory>

class ParseEvent; // Forward declaration for ParseEvent, used in processRoom
class Path;
// class RoomSignalHandler; // Removed forward declaration
struct PathParameters;

class NODISCARD OneByOne final : public Experimenting
{
public: // Added processRoom
    void processRoom(const RoomHandle &room, const ParseEvent &event) override;

private:
    SharedParseEvent event;
    // RoomSignalHandler *handler = nullptr; // Removed member

public:
    explicit OneByOne(const SigParseEvent &sigParseEvent,
                      PathParameters &in_params); // Removed handler from constructor

private:
    // void virt_receiveRoom(const RoomHandle &room) final; // Removed

public:
    void addPath(std::shared_ptr<Path> path);
};
