#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include "../map/ChangeList.h" // For ChangeList and Change
#include "../map/roomid.h"     // For RoomId
#include <unordered_map>

// Forward declarations
class SigParseEvent;
class MapFrontend;
class Change;

namespace mmapper {

enum class PendingRoomOperation : uint8_t {
    NONE = 0,
    MAKE_PERMANENT,
    REMOVE_ROOM
};

struct PathEventContext {
    SigParseEvent &currentEvent;
    ChangeList &changes;
    std::unordered_map<RoomId, PendingRoomOperation> pendingRoomOperations;
    MapFrontend &map;

    PathEventContext(SigParseEvent &event, ChangeList &cl, MapFrontend &mapRef)
        : currentEvent(event)
        , changes(cl)
        , map(mapRef) {}

    // Declaration for addTrackedChange. Implementation will be in a .cpp file or inline later.
    void addTrackedChange(const Change &change);

    // Optional helper methods (declarations) - implementation can be added if needed
    bool isOperationPending(RoomId id, PendingRoomOperation op) const;
    void recordOperation(RoomId id, PendingRoomOperation op);
    PendingRoomOperation getPendingOperation(RoomId id) const;
};

} // namespace mmapper
