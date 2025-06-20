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
#include <memory> // Added for std::enable_shared_from_this

#include <unordered_map>

class MapFrontend;
class ParseEvent;

/**
 * @brief PathProcessor strategy for the "Approved" pathfinding state.
 *
 * Used when PathMachine is confident of the current room. It attempts to find
 * a single, unambiguous match for incoming event data among directly accessible
 * rooms or by server ID. Manages temporary room cleanup via ChangeList if
 * rooms don't match or if multiple matches occur.
 */
class NODISCARD Approved final : public PathProcessor, public std::enable_shared_from_this<Approved>
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

    // Overrides for getSharedPtrFromThis
    std::shared_ptr<PathProcessor> getSharedPtrFromThis() override;
    std::shared_ptr<const PathProcessor> getSharedPtrFromThis() const override;
};
