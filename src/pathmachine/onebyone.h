#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "../map/parseevent.h"
#include "../map/RoomIdSet.h"
#include "experimenting.h" // Provides PathList for m_new_paths type
#include "path.h"          // Provides Path for m_path type

#include <memory>

class ParseEvent;
// Path class declaration is now included via path.h
class RoomSignalHandler;
struct PathParameters;

class NODISCARD OneByOne final : public Experimenting
{
private:
    SharedParseEvent event;
    RoomSignalHandler *handler = nullptr;
    RoomIdSet m_collectedRoomIds;
    std::shared_ptr<Path> m_path; // Current parent path for evaluate
    std::shared_ptr<PathList> m_new_paths; // List of new paths generated

public:
    explicit OneByOne(const SigParseEvent &sigParseEvent,
                      PathParameters &in_params,
                      RoomSignalHandler *handler);

public:
    void addPath(std::shared_ptr<Path> path);
    // OneByOne provides its own evaluate, hiding Experimenting::evaluate
    std::shared_ptr<PathList> evaluate();
};
