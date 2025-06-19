#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "../global/RuleOf5.h"
#include "../map/RoomHandle.h"
// #include "../map/RoomRecipient.h" // Removed
#include "../map/parseevent.h"
#include "../map/room.h"
#include "pathmachine.h" // Added
#include "../map/roomid.h"

#include <unordered_map>

class MapFrontend;
class ParseEvent; // Forward declaration still needed if pathmachine.h doesn't provide it directly for this context

class NODISCARD Approved final : public PathProcessor // Inherit from PathProcessor
{
private:
    SigParseEvent myEvent;
    std::unordered_map<RoomId, ComparisonResultEnum> compareCache;
    RoomHandle matchedRoom;
    MapFrontend &m_map;
    const int matchingTolerance;
    bool moreThanOne = false;
    bool update = false;

public:
    explicit Approved(MapFrontend &map, const SigParseEvent &sigParseEvent, int matchingTolerance);
    ~Approved() final;

public:
    Approved() = delete;
    DELETE_CTORS_AND_ASSIGN_OPS(Approved);

public: // Changed to public and new signature
    void processRoom(const RoomHandle &room, const ParseEvent &event) override;

public:
    NODISCARD RoomHandle oneMatch() const;
    NODISCARD bool needsUpdate() const { return update; }
    void releaseMatch();
};
