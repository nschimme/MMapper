#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "../map/parseevent.h"
#include "experimenting.h"
#include "../map/ChangeList.h"   // Added for ChangeList
#include <memory> // Already present, needed for std::enable_shared_from_this


class ParseEvent;
class Path;
class RoomSignalHandler;
struct PathParameters;

class NODISCARD OneByOne final : public Experimenting, public std::enable_shared_from_this<OneByOne>
{
private:
    SharedParseEvent m_event; // Prefixed
    RoomSignalHandler &m_handler; // Prefixed

public:
    explicit OneByOne(const SigParseEvent &sigParseEvent,
                      PathParameters &in_params,
                      RoomSignalHandler &handler); // Parameter name unchanged, will be used to init m_handler

private:
    void virt_receiveRoom(const RoomHandle &room, ChangeList &changes) final;

public:
    void addPath(std::shared_ptr<Path> path);

    // Overrides for getSharedPtrFromThis
    std::shared_ptr<PathProcessor> getSharedPtrFromThis() override;
    std::shared_ptr<const PathProcessor> getSharedPtrFromThis() const override;
};
