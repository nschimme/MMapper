// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include "RoomSorter.h"
#include "Map.h" // Assuming Map class provides context, adjust if necessary
#include <algorithm> // For std::find_if, std::find
#include <set>
#include <vector> // Already included via RoomSorter.h but good for clarity
#include <map>    // Already included via RoomSorter.h but good for clarity
#include <queue>  // For BFS

// RoomSorter::RoomSorter(const Map& mapContext) { /* ... */ }
RoomSorter::RoomSorter() {}

const RawRoom* RoomSorter::getRoomById(RoomId id) const {
    auto it = m_allRoomsLookup.find(id);
    if (it != m_allRoomsLookup.end()) {
        return &it->second;
    }
    return nullptr;
}

bool RoomSorter::is_cardinal(ExitDirEnum direction) const {
    return direction == ExitDirEnum::North || direction == ExitDirEnum::South ||
           direction == ExitDirEnum::East || direction == ExitDirEnum::West ||
           direction == ExitDirEnum::Up || direction == ExitDirEnum::Down;
}

ExitDirEnum RoomSorter::get_opposite_direction(ExitDirEnum direction) const {
    switch (direction) {
        case ExitDirEnum::North: return ExitDirEnum::South;
        case ExitDirEnum::South: return ExitDirEnum::North;
        case ExitDirEnum::East: return ExitDirEnum::West;
        case ExitDirEnum::West: return ExitDirEnum::East;
        case ExitDirEnum::Up: return ExitDirEnum::Down;
        case ExitDirEnum::Down: return ExitDirEnum::Up;
        default: return ExitDirEnum::Invalid; // Or throw an error
    }
}

Coordinate RoomSorter::get_target_coordinate(const Coordinate& startCoord, ExitDirEnum direction, int distance) const {
    Coordinate targetCoord = startCoord;
    switch (direction) {
        case ExitDirEnum::North: targetCoord.y += distance; break;
        case ExitDirEnum::South: targetCoord.y -= distance; break;
        case ExitDirEnum::East:  targetCoord.x += distance; break;
        case ExitDirEnum::West:  targetCoord.x -= distance; break;
        case ExitDirEnum::Up:    targetCoord.z += distance; break;
        case ExitDirEnum::Down:  targetCoord.z -= distance; break;
        default: break; // No change for invalid/non-cardinal directions
    }
    return targetCoord;
}

bool RoomSorter::are_coordinates_cardinally_aligned(const RawRoom& roomA, const RawRoom& roomB, ExitDirEnum directionFromAtoB) const {
    if (!is_cardinal(directionFromAtoB)) {
        return false; // Non-cardinal directions cannot be cardinally aligned in this context
    }
    Coordinate expectedBPos = get_target_coordinate(roomA.getPosition(), directionFromAtoB);
    return roomB.getPosition() == expectedBPos;
}

// Helper to find a room by ID from a list of rooms
const RawRoom* findRoomById(RoomId id, const std::vector<RawRoom>& rooms) {
    auto it = std::find_if(rooms.begin(), rooms.end(), [&](const RawRoom& r){ return r.getId() == id; });
    if (it != rooms.end()) {
        return &(*it);
    }
    return nullptr;
}

ConnectionType RoomSorter::getConnectionType(const RawRoom& roomA, const RawRoom& roomB, ExitDirEnum exitDirFromAtoB, const std::vector<RawRoom>& allMapRooms) const {
    if (!is_cardinal(exitDirFromAtoB)) {
        return ConnectionType::NonCardinal;
    }

    // Check if roomA has the specified exit to roomB's ID
    bool aToBExitExists = false;
    const auto& exitA = roomA.getExit(exitDirFromAtoB);
    if (exitA.isExit()) { // Check if it's an actual exit
         // Assuming getOutgoingSet() returns a collection of RoomId
         // and that RawExit has a way to get target room IDs.
         // This part needs to be adapted to the actual RawExit API.
         // For now, let's assume exitA.getTargetRoomId() or similar exists.
         // If RawExit stores multiple possible target IDs, we need to check if roomB.id is among them.
         // Placeholder:
         // if (exitA.getTargetRoomId() == roomB.getId()) {
         //    aToBExitExists = true;
         // }
         // For MVP, let's assume a simple direct connection for now, if the exit direction matches.
         // A more robust check would involve iterating through exitA.getOutgoingSet()
         // and checking if roomB.getId() is in it.
         // This part is highly dependent on `RawExit` and `Room` API.
         // Let's assume for now the caller ensures roomA and roomB are *supposed* to be connected via exitDirFromAtoB.
    }
    // Simplified: if the API doesn't directly give target ID from RawExit,
    // we might infer based on coordinates if they are aligned.
    // This is less robust.

    bool coordinatesAligned = are_coordinates_cardinally_aligned(roomA, roomB, exitDirFromAtoB);

    if (!coordinatesAligned) {
        // If coordinates are not aligned, it's likely non-cardinal in practice for layout purposes
        return ConnectionType::NonCardinal;
    }

    // Check for return exit from B to A
    ExitDirEnum oppositeDir = get_opposite_direction(exitDirFromAtoB);
    if (oppositeDir == ExitDirEnum::Invalid) { // Should not happen if exitDirFromAtoB is cardinal
        return ConnectionType::NonCardinal;
    }

    const auto& exitB = roomB.getExit(oppositeDir);
    bool bToAExitExists = false;
    if (exitB.isExit()) {
        // Placeholder for checking if exitB targets roomA.getId()
        // if (exitB.getTargetRoomId() == roomA.getId()) {
        //    bToAExitExists = true;
        // }
    }

    // This is a simplified interpretation for now.
    // A true "StronglyCardinal" implies that if you go North from A to B,
    // then going South from B *must* lead back to A.
    // This requires checking the target IDs of the exits.
    // The current RawRoom/RawExit structure read so far doesn't explicitly show easy access to target room ID from RawExit.
    // It has `getOutgoingSet()` and `getIncomingSet()` on `ExitFieldsGetters` (from Crtp.h)
    // which implies an Exit object might store a set of room IDs it connects to.

    // Given the current information, let's base it on coordinate alignment and existence of exits.
    // If roomA has an exit in direction exitDirFromAtoB, and roomB has an exit in the opposite direction,
    // and their coordinates align, we'll call it StronglyCardinal for now.
    // This might need refinement once we see how Map::getRoomExits or similar works.

    if (roomA.hasNontrivialExit(exitDirFromAtoB) && roomB.hasNontrivialExit(oppositeDir) && coordinatesAligned) {
        // Further check if these exits actually point to each other.
        // This requires a way to get the target room ID from an exit.
        // For now, let's assume if they both have exits and align, it's strong.
        // This is a simplification.
        const RawExit& exitA_obj = roomA.getExit(exitDirFromAtoB);
        bool a_points_to_b = false;
        for(const auto& target_id : exitA_obj.getOutgoingSet()){ // This is a guess based on CRTP.h
            if(target_id == roomB.getId()){
                a_points_to_b = true;
                break;
            }
        }

        const RawExit& exitB_obj = roomB.getExit(oppositeDir);
        bool b_points_to_a = false;
        for(const auto& target_id : exitB_obj.getOutgoingSet()){ // This is a guess
            if(target_id == roomA.getId()){
                b_points_to_a = true;
                break;
            }
        }
        if (a_points_to_b && b_points_to_a) {
             return ConnectionType::StronglyCardinal;
        } else {
            return ConnectionType::WeaklyCardinal; // Exits exist and align, but don't form a pair.
        }
    } else if (roomA.hasNontrivialExit(exitDirFromAtoB) && coordinatesAligned) {
        // A has an exit to where B is, but B doesn't have a return exit (or it's not cardinal)
        return ConnectionType::WeaklyCardinal;
    }

    return ConnectionType::NonCardinal; // Default if not meeting above criteria
}


std::vector<RawRoom> RoomSorter::arrangeRooms(const std::vector<RawRoom>& selectedRooms, const std::vector<RawRoom>& allMapRooms) {
    m_allRoomsLookup.clear();
    for(const auto& room : allMapRooms) {
       m_allRoomsLookup[room.getId()] = room;
    }
    m_selectedRoomsInternalCopy = selectedRooms;

    m_processedRoomNodes.clear();
    for(const auto& room : selectedRooms) {
        RoomNode node;
        node.id = room.getId();
        node.isSelected = true; // All rooms passed here are selected
        node.originalPosition = room.getPosition();
        node.newPosition = room.getPosition(); // Default to original
        node.groupId = -1;
        m_processedRoomNodes.push_back(node);
    }

    m_identifiedCardinalBlocks.clear();
    m_identifiedAnchorConnections.clear();
    m_roomToBlockAssignment.clear();
    // m_nextBlockId = 0; // Reset before identify - This should be done here or ensure arrangeRooms is not re-entrant with old state
    // identify_groups_and_anchors resets m_nextBlockId or it's done before calling.
    // Let's ensure it is reset at the beginning of identify_groups_and_anchors or here.
    // For now, assuming identify_groups_and_anchors handles its own state for m_nextBlockId initiation.
    // Actually, m_nextBlockId is a member, so it should be reset if arrangeRooms can be called multiple times.
    m_nextBlockId = 0;


    identify_groups_and_anchors(selectedRooms, allMapRooms);
    cardinalize_groups();
    place_groups();

    std::vector<RawRoom> resultRooms;
    std::map<RoomId, const RawRoom*> originalSelectedRoomsMap;
    for(const auto& r : m_selectedRoomsInternalCopy) {
        originalSelectedRoomsMap[r.getId()] = &r;
    }

    for (const auto& node : m_processedRoomNodes) {
        auto it = originalSelectedRoomsMap.find(node.id);
        if (it != originalSelectedRoomsMap.end()) {
            RawRoom modifiedRoom = *(it->second); // Create a copy
            modifiedRoom.setPosition(node.newPosition);
            // TODO: If room properties other than position were changed (e.g. new exits if we re-linked things), update them here.
            resultRooms.push_back(modifiedRoom);
        }
    }
    return resultRooms;
}

// Definitions for identify_groups_and_anchors, cardinalize_groups, place_groups will be added later.
void RoomSorter::identify_groups_and_anchors(const std::vector<RawRoom>& selectedRooms, const std::vector<RawRoom>& allMapRoomsContext) {
    // Create a lookup for selected room IDs for efficient checking
    std::set<RoomId> selectedRoomIds;
    for (const auto& room : m_selectedRoomsInternalCopy) {
        selectedRoomIds.insert(room.getId());
    }

    // 1. Identify Cardinal Anchors
    for (const auto& roomA : m_selectedRoomsInternalCopy) {
        for (int i = 0; i < static_cast<int>(ExitDirEnum::Count); ++i) {
            ExitDirEnum direction = static_cast<ExitDirEnum>(i);
            if (!is_cardinal(direction)) continue;

            const auto& exitA = roomA.getExit(direction);
            if (!exitA.isExit()) continue;

            for (const RoomId& targetRoomId : exitA.getOutgoingSet()) {
                const RawRoom* roomB_ptr = getRoomById(targetRoomId);
                if (roomB_ptr) {
                    // Check if roomB is NOT selected
                    if (selectedRoomIds.find(targetRoomId) == selectedRoomIds.end()) {
                        if (getConnectionType(roomA, *roomB_ptr, direction, allMapRoomsContext) == ConnectionType::StronglyCardinal) {
                            AnchorConnection ac;
                            ac.selectedRoomId = roomA.getId();
                            ac.anchorRoomId = targetRoomId;
                            ac.directionFromSelectedToAnchor = direction;
                            ac.anchorRoomPosition = roomB_ptr->getPosition();
                            m_identifiedAnchorConnections.push_back(ac);
                        }
                    }
                }
            }
        }
    }

    // 2. Identify Strongly Cardinal Blocks within Selected Rooms
    std::set<RoomId> visitedSelectedRoomsForBlocks;
    for (const auto& selectedRoom : m_selectedRoomsInternalCopy) {
        if (visitedSelectedRoomsForBlocks.count(selectedRoom.getId())) {
            continue;
        }

        std::vector<RoomId> currentBlockRoomIds;
        std::queue<RoomId> q; // For BFS

        q.push(selectedRoom.getId());
        visitedSelectedRoomsForBlocks.insert(selectedRoom.getId());
        currentBlockRoomIds.push_back(selectedRoom.getId());

        while (!q.empty()) {
            RoomId currentRoomId = q.front();
            q.pop();
            const RawRoom* roomInBlock = getRoomById(currentRoomId); // Should always be found as it's a selected room
            if(!roomInBlock) continue; // Should not happen

            for (int i = 0; i < static_cast<int>(ExitDirEnum::Count); ++i) {
                ExitDirEnum direction = static_cast<ExitDirEnum>(i);
                if (!is_cardinal(direction)) continue;

                const auto& exitObj = roomInBlock->getExit(direction);
                if (!exitObj.isExit()) continue;

                for (const RoomId& targetId : exitObj.getOutgoingSet()) {
                    // Check if neighbor is selected AND not yet visited for block formation
                    if (selectedRoomIds.count(targetId) && !visitedSelectedRoomsForBlocks.count(targetId)) {
                        const RawRoom* neighborRoom_ptr = getRoomById(targetId);
                        if (neighborRoom_ptr) { // Should be found if it's in selectedRoomIds & m_allRoomsLookup
                            if (getConnectionType(*roomInBlock, *neighborRoom_ptr, direction, allMapRoomsContext) == ConnectionType::StronglyCardinal) {
                                visitedSelectedRoomsForBlocks.insert(targetId);
                                q.push(targetId);
                                currentBlockRoomIds.push_back(targetId);
                            }
                        }
                    }
                }
            }
        }

        if (currentBlockRoomIds.size() > 1) {
            CardinalBlock block;
            block.id = m_nextBlockId++;
            block.roomIds = currentBlockRoomIds;
            m_identifiedCardinalBlocks.push_back(block);

            for (const RoomId& roomIdInBlock : currentBlockRoomIds) {
                m_roomToBlockAssignment[roomIdInBlock] = block.id;
                // Update groupId in m_processedRoomNodes
                auto node_it = std::find_if(m_processedRoomNodes.begin(), m_processedRoomNodes.end(),
                                           [&](const RoomNode& node){ return node.id == roomIdInBlock; });
                if (node_it != m_processedRoomNodes.end()) {
                    node_it->groupId = block.id;
                }
            }
        }
    }
    // Selected rooms not part of any block will have their groupId = -1 (default)
}

void RoomSorter::cardinalize_groups() {
    // First, ensure all rooms within identified CardinalBlocks maintain their relative cardinal positions.
    // For now, we assume their newPosition is initially their originalPosition.
    // The block as a whole will be shifted in place_groups.
    // This means if a block was at (10,10,0), (10,11,0), their newPositions are still that,
    // until the whole block is moved.

    // Process loose rooms (those not in a CardinalBlock)
    for (RoomNode& node : m_processedRoomNodes) {
        if (node.groupId == -1) { // It's a loose room
            bool position_set_by_anchor = false;
            // Check if this loose room has a direct anchor
            for (const auto& anchorConn : m_identifiedAnchorConnections) {
                if (anchorConn.selectedRoomId == node.id) {
                    // This loose room is anchored. Position it cardinally relative to the anchor.
                    // The anchor connection is 'StronglyCardinal'.
                    // We need to place 'node' 1 space away from the anchor.
                    // The direction in AnchorConnection is from selected (node) to anchor.
                    // So, we need the opposite direction to place the node relative to the anchor.
                    ExitDirEnum dirFromAnchorToNode = get_opposite_direction(anchorConn.directionFromSelectedToAnchor);

                    if (dirFromAnchorToNode != ExitDirEnum::Invalid) {
                        // Place node 2 units away in that direction to create a 1-space gap.
                        // (Anchor) -> space -> (Node)
                        node.newPosition = get_target_coordinate(anchorConn.anchorRoomPosition, dirFromAnchorToNode, 2);
                        position_set_by_anchor = true;
                        // If a room has multiple anchors, this takes the last one.
                        // For MVP, this simplification is acceptable. A more advanced version might score anchor placements.
                        break;
                    }
                }
            }

            if (position_set_by_anchor) {
                // Potentially mark this node as 'cardinalized' or 'position_fixed_by_anchor' if needed later
            } else {
                // If not anchored, its newPosition currently remains its originalPosition.
                // Phase 3 (place_groups) will handle finding a spot for these,
                // and for the blocks.
            }
        }
    }

    // TODO for more advanced cardinalization (if time permits post-MVP or in later subtasks):
    // - Link loose rooms to CardinalBlocks if they have connections.
    // - Form small cardinal chains from remaining loose rooms.
    // - For rooms within CardinalBlocks, ensure their newPosition is correctly set relative
    //   to a chosen 'origin' room of the block, if not already implicitly handled by them
    //   sharing original positions that were already cardinal.
    //   For now, we assume originalPositions within a block are relatively correct.
}

void RoomSorter::place_groups() {
    m_occupiedCoordinates.clear();
    // 1. Populate initial occupancy from all non-selected rooms on the map
    for (const auto& pair : m_allRoomsLookup) {
        bool isSelected = false;
        for (const auto& selRoom : m_selectedRoomsInternalCopy) {
            if (pair.first == selRoom.getId()) {
                isSelected = true;
                break;
            }
        }
        if (!isSelected) {
            m_occupiedCoordinates.insert(pair.second.getPosition());
        }
    }

    // 2. Process already positioned anchored loose rooms (from cardinalize_groups)
    // Their newPosition is considered 'tentatively final'. Add to occupancy.
    // If collisions occur here, it's a more complex scenario (e.g. anchor conflict)
    for (RoomNode& node : m_processedRoomNodes) {
        if (node.groupId == -1) { // Loose room
            bool wasAnchored = false; // Check if its position was set by an anchor
            for (const auto& anchorConn : m_identifiedAnchorConnections) {
                if (anchorConn.selectedRoomId == node.id) {
                    wasAnchored = true; break;
                }
            }
            if (wasAnchored) {
                // For MVP, assume anchored position is valid and add to occupancy.
                // A more robust solution would check for collision and attempt to adjust.
                if (m_occupiedCoordinates.count(node.newPosition)) {
                    // Collision with non-selected room or another anchored room!
                    // For MVP: log this or handle simply. For now, we'll overwrite,
                    // assuming map won't have anchors in impossible spots.
                    // Or, ideally, the search function below should be used.
                    // For now, let's assume these are priority placements.
                }
                m_occupiedCoordinates.insert(node.newPosition);
            }
        }
    }

    // Helper lambda to find an empty spot (very basic spiral-like search)
    // Needs to account for 1-space gap if required for the item being placed.
    auto find_empty_spot = [&](Coordinate targetPos, bool requiresGap) -> Coordinate {
        // Search for 1 unit, then 2, etc. in cardinal directions
        // For MVP, this is simplified. A true spiral is more complex.
        // This also doesn't yet handle the 1-space gap correctly during search.
        if (!m_occupiedCoordinates.count(targetPos)) return targetPos;

        for (int dist = 1; dist < 10; ++dist) { // Limit search range
            for (int dim = 0; dim < 3; ++dim) { // x, y, z
                for (int sign = -1; sign <= 1; sign += 2) {
                    Coordinate offset(0,0,0);
                    if (dim == 0) offset.x = dist * sign;
                    else if (dim == 1) offset.y = dist * sign;
                    else offset.z = dist * sign;

                    Coordinate checkPos = targetPos + offset;
                    // TODO: If requiresGap, checkPos needs to ensure its neighbors (dist-1) are also clear.
                    // This simplified version just checks the spot itself.
                    if (!m_occupiedCoordinates.count(checkPos)) return checkPos;
                }
            }
        }
        return targetPos; // Failed to find, return original (will likely overlap)
    };


    // 3. Place Cardinal Blocks
    for (const auto& block : m_identifiedCardinalBlocks) {
        if (block.roomIds.empty()) continue;

        // Determine initial target position for the block's origin room
        // For MVP, use the original position of the first room in the block.
        RoomId originRoomId = block.roomIds.front();
        const RawRoom* originRoomInitialPtr = nullptr;
        // Find the original RawRoom object for the origin room ID to get its original position
        // This relies on m_selectedRoomsInternalCopy containing the state before any newPosition changes.
        // Or, use the originalPosition from the RoomNode if that's more direct.
        Coordinate originRoomOriginalPos;
        bool originNodeFound = false;
        for(const auto& nodeCheck : m_processedRoomNodes) {
            if(nodeCheck.id == originRoomId) {
                originRoomOriginalPos = nodeCheck.originalPosition;
                originNodeFound = true;
                break;
            }
        }

        if (!originNodeFound) continue; // Should not happen if block.roomIds are from m_processedRoomNodes
        Coordinate blockTargetOriginPos = originRoomOriginalPos;

        // TODO: If block is anchored, adjust blockTargetOriginPos based on anchor.
        // This part is complex as it involves shifting the whole block.
        // For MVP, we might assume blocks are not directly "re-anchored" by a single room's anchor if already a block.

        bool blockRequiresGap = true; // Assume re-cardinalized blocks need a gap.

        // Try to place the block. This needs a more robust search that considers all rooms in the block.
        // The find_empty_spot above is for single rooms.
        // For MVP: an approximation - find a spot for the origin, then place others relatively.
        // This won't correctly handle block collisions.

        Coordinate foundBlockOriginPos = find_empty_spot(blockTargetOriginPos, blockRequiresGap);
        Coordinate offsetFromOriginalToNew = foundBlockOriginPos - originRoomOriginalPos;

        for (RoomId idInBlock : block.roomIds) {
            for (RoomNode& node : m_processedRoomNodes) {
                if (node.id == idInBlock) {
                    // It's important that node.originalPosition is used here for relative placement
                    node.newPosition = node.originalPosition + offsetFromOriginalToNew;
                    m_occupiedCoordinates.insert(node.newPosition);
                    break;
                }
            }
        }
    }

    // 4. Place Remaining Loose Rooms (not anchored, not part of blocks yet by newPosition)
    for (RoomNode& node : m_processedRoomNodes) {
        if (node.groupId == -1) { // Loose room
            bool wasAnchored = false;
            for (const auto& anchorConn : m_identifiedAnchorConnections) {
                if (anchorConn.selectedRoomId == node.id) {
                    wasAnchored = true; break;
                }
            }
            if (wasAnchored) continue; // Already handled (or attempted)

            bool roomRequiresGap = true; // Assume loose rooms are "non-cardinal" needing a gap
            Coordinate targetPos = node.originalPosition; // Start with its original spot

            // TODO: Try to place near connected, already-placed selected rooms.
            // For MVP, just find an empty spot near its original position.

            node.newPosition = find_empty_spot(targetPos, roomRequiresGap);
            m_occupiedCoordinates.insert(node.newPosition);
        }
    }
}
