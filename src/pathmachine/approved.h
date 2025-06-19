#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "../global/RuleOf5.h"
#include "../map/RoomHandle.h"
// RoomRecipient.h is removed
#include "../map/parseevent.h"
#include "../map/room.h"
#include "../map/roomid.h"
#include "../map/Change.h" // Added Change.h

#include <unordered_map>
#include <vector> // Added vector

class MapFrontend;
class ParseEvent;

class NODISCARD Approved final // Removed inheritance from RoomRecipient
{
private:
    std::vector<RoomId> m_rooms_to_remove; // Added m_rooms_to_remove
    SigParseEvent myEvent;
    std::unordered_map<RoomId, ComparisonResultEnum> compareCache;
    RoomHandle matchedRoom;
    MapFrontend &m_map;
    const int matchingTolerance;
    bool moreThanOne = false;
    bool update = false;

public:
    explicit Approved(MapFrontend &map, const SigParseEvent &sigParseEvent, int matchingTolerance);
    // ~Approved() final; // Destructor removed

public:
    Approved() = delete;
    DELETE_CTORS_AND_ASSIGN_OPS(Approved);

private:
    void processRoom(const RoomHandle &); // Renamed virt_receiveRoom and removed final

public:
    std::vector<Change> finalizeChanges(); // Added finalizeChanges()
    NODISCARD RoomHandle oneMatch() const;
    NODISCARD bool needsUpdate() const { return update; }
    void releaseMatch();
};
