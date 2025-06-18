#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "../global/RuleOf5.h"
#include "../map/RoomHandle.h"
#include "../map/RoomIdSet.h" // Added RoomIdSet.h
#include "../map/parseevent.h"
#include "../map/room.h"
#include "../map/roomid.h"

#include <unordered_map>

class MapFrontend;
class ParseEvent;

class NODISCARD Approved // Removed RoomRecipient inheritance
{
private:
    SigParseEvent myEvent;
    std::unordered_map<RoomId, ComparisonResultEnum> compareCache;
    RoomHandle matchedRoom; // This might be redundant if oneMatch directly uses m_collectedRoomIds
    MapFrontend &m_map;
    const int matchingTolerance;
    bool moreThanOne = false; // This might be deducible from m_collectedRoomIds.size() > 1
    bool update = false; // Renamed from m_needs_update in prompt, assuming this is it
    RoomIdSet m_collectedRoomIds; // Added

public:
    explicit Approved(MapFrontend &map, const SigParseEvent &sigParseEvent, int matchingTolerance);
    ~Approved(); // Removed final as it's no longer overriding

public:
    Approved() = delete;
    DELETE_CTORS_AND_ASSIGN_OPS(Approved);

    // Added public getter
    const RoomIdSet& getCollectedRoomIds() const { return m_collectedRoomIds; }

    // receiveRoom methods will be modified in .cpp to not be virtual overrides
    // virt_receiveRoom removed as it was part of RoomRecipient interface

public:
    NODISCARD RoomHandle oneMatch() const; // Return type is already RoomHandle
    NODISCARD bool needsUpdate() const { return update; }
    void releaseMatch();
};
