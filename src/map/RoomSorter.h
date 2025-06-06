#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include "RawRoom.h"
#include "ExitDirection.h"
#include "coordinate.h"
#include <vector>
#include <map>
#include <set>
#include <algorithm> // For std::min/max in BoundingBox

// Forward declaration if Map class is needed and causes include issues
// class Map;

enum class ConnectionType {
    StronglyCardinal,
    WeaklyCardinal,
    NonCardinal,
    Unknown // For cases where rooms are not directly connected
};

class RoomSorter {
public:
    struct RoomNode {
        RoomId id;
        bool isSelected;
        Coordinate originalPosition;
        Coordinate newPosition;
        int groupId = -1;
    };

    struct CardinalBlock {
        int id;
        std::vector<RoomId> roomIds;
        // Could add a representative position for the block later
    };

    struct AnchorConnection {
        RoomId selectedRoomId;
        RoomId anchorRoomId;
        ExitDirEnum directionFromSelectedToAnchor;
        Coordinate anchorRoomPosition;
    };

    // Constructor might take existing map data if needed for context (e.g., anchors)
    // RoomSorter(const Map& mapContext);
    RoomSorter(); // Simplified constructor for now

    // Main function to arrange rooms
    std::vector<RawRoom> arrangeRooms(const std::vector<RawRoom>& selectedRooms, const std::vector<RawRoom>& allMapRooms);

private:
    const RawRoom* getRoomById(RoomId id) const; // Helper to get room from m_allRoomsLookup
    // Helper functions defined in Plan Step 1
    bool is_cardinal(ExitDirEnum direction) const;
    ExitDirEnum get_opposite_direction(ExitDirEnum direction) const;
    bool are_coordinates_cardinally_aligned(const RawRoom& roomA, const RawRoom& roomB, ExitDirEnum directionFromAtoB) const;
    ConnectionType getConnectionType(const RawRoom& roomA, const RawRoom& roomB, ExitDirEnum exitDirFromAtoB, const std::vector<RawRoom>& allMapRooms) const;
    Coordinate get_target_coordinate(const Coordinate& startCoord, ExitDirEnum direction, int distance = 1) const;

    // Placeholder for the actual algorithm phases
    void identify_groups_and_anchors(const std::vector<RawRoom>& selectedRooms, const std::vector<RawRoom>& allMapRooms);
    void cardinalize_groups();
    void place_groups();

    // Member variables
    std::map<RoomId, RawRoom> m_allRoomsLookup;
    std::vector<RawRoom> m_selectedRoomsInternalCopy; // To store a copy of selected rooms

    std::vector<RoomNode> m_processedRoomNodes; // Stores nodes for selected rooms
    std::vector<CardinalBlock> m_identifiedCardinalBlocks;
    std::vector<AnchorConnection> m_identifiedAnchorConnections;
    std::map<RoomId, int> m_roomToBlockAssignment; // Map RoomId to CardinalBlock.id
    int m_nextBlockId = 0;
    // Example: To store processed rooms or intermediate state
    // std::vector<RawRoom> m_processedRooms;
};
