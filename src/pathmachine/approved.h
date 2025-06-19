#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "../global/RuleOf5.h"
#include "../map/RoomHandle.h"
#include "PathProcessor.h" // Changed path
#include "../map/parseevent.h"
#include "../map/room.h"
#include "../map/roomid.h"
#include "../map/ChangeList.h" // Added for ChangeList
#include "../global/WeakHandle.h" // Added for EnableGetWeakHandleFromThis

#include <unordered_map>

class MapFrontend;
class ParseEvent;

class NODISCARD Approved final : public PathProcessor, public EnableGetWeakHandleFromThis<Approved>
{
private:
    SigParseEvent m_myEvent; // Prefixed
    std::unordered_map<RoomId, ComparisonResultEnum> m_compareCache; // Prefixed
    RoomHandle m_matchedRoom; // Prefixed
    MapFrontend &m_map; // Already prefixed
    const int m_matchingTolerance; // Prefixed
    bool m_moreThanOne = false; // Prefixed
    bool m_update = false; // Prefixed

public:
    explicit Approved(MapFrontend &map, const SigParseEvent &sigParseEvent, int matchingTolerance);

public:
    Approved() = delete;
    DELETE_CTORS_AND_ASSIGN_OPS(Approved);

private:
    void virt_receiveRoom(const RoomHandle &, ChangeList &changes) final;

public:
    NODISCARD RoomHandle oneMatch() const;
    NODISCARD bool needsUpdate() const { return m_update; } // Prefixed
    void releaseMatch(ChangeList &changes);
};
