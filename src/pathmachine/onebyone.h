#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "../map/parseevent.h"
#include "experimenting.h"

#include <memory>

// class ParseEvent; // Removed forward declaration
class Path;
// class RoomSignalHandler; // Removed forward declaration
struct PathParameters;

class NODISCARD OneByOne final : public Experimenting
{
public: // Updated processRoom signature
    void processRoom(const RoomHandle &room) override;

private:
    SharedParseEvent event; // This class uses its own 'event' member, initialized from SigParseEvent
    // RoomSignalHandler *handler = nullptr; // Removed member

public:
    explicit OneByOne(const SigParseEvent &sigParseEvent,
                      PathParameters &in_params); // Removed handler from constructor

private:
    // void virt_receiveRoom(const RoomHandle &room) final; // Removed

public:
    void addPath(std::shared_ptr<Path> path);
};
