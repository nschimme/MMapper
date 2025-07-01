#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "../map/roomid.h"
#include "../map/parseevent.h" // For SigParseEvent
#include "../map/RoomHandle.h"  // For RoomHandle
#include <optional>
#include <set>

// Forward declarations
class MapFrontend;
class Coordinate;
class SigParseEvent;
class RoomHandle;
enum class ServerRoomId : uint32_t; // Assuming ServerRoomId is an enum class or strong type

class NODISCARD PathContext
{
public:
    explicit PathContext(MapFrontend& mapFrontend);

    // Room creation, deletion, and permanence requests
    std::optional<RoomId> requestCreateRoom(const SigParseEvent& event, const Coordinate& coord);
    void requestDeleteTemporaryRoom(RoomId id);
    void requestMakeRoomPermanent(RoomId id);

    // State queries
    bool isRoomPendingDeletion(RoomId id) const;
    bool isRoomPendingPermanent(RoomId id) const;
    bool wasRoomCreatedThisCycle(RoomId id) const;

    // Passthrough finders (delegating to MapFrontend)
    RoomHandle findRoomHandle(RoomId id) const;
    RoomHandle findRoomHandle(const Coordinate& coord) const;
    RoomHandle findRoomHandle(ServerRoomId id) const;
    // Add other findRoomHandle overloads if necessary, e.g., for ExternalRoomId

    // Appends accumulated room lifecycle changes (deletions, permanence) to the provided ChangeList.
    // Performs staleness checks before adding changes.
    void flushChanges(ChangeList& outChangeList);

    // Disable copy and move
    PathContext(const PathContext&) = delete;
    PathContext& operator=(const PathContext&) = delete;
    PathContext(PathContext&&) = delete;
    PathContext& operator=(PathContext&&) = delete;

private:
    MapFrontend& m_mapFrontend;
    std::set<RoomId> m_roomsCreatedThisCycle;
    std::set<RoomId> m_roomsPendingDeletion;
    std::set<RoomId> m_roomsPendingPermanent;
};
