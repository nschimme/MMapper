// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include <QtTest/QtTest>
#include "../src/map/RoomSorter.h" // Adjust path as necessary
#include "../src/map/RawRoom.h"
#include "../src/map/RawExit.h"
#include "../src/map/ExitDirection.h"
#include "../src/map/coordinate.h"
#include "../src/map/roomid.h" // For RoomId, INVALID_ROOMID
#include <vector>
#include <iostream> // For QDebug-like output if needed via std::cout
#include <cmath>    // For std::abs

// Helper function to create a basic RawRoom for testing
RawRoom createTestRoom(RoomId id, Coordinate pos) {
    RawRoom room;
    room.setId(id);
    room.setPosition(pos);
    // Set other necessary fields to default/valid states if RoomSorter depends on them
    // For example, name, area, etc. from RoomFields.
    // room.fields.setArea(RoomArea("TestArea"));
    // room.fields.setName(RoomName("TestRoom"));
    return room;
}

// Helper to add a bidirectional exit between two rooms for testing
// This is a simplified helper. Real exit creation might be more complex.
void addBidirectionalExit(RawRoom& roomA, RawRoom& roomB, ExitDirEnum dirAtoB) {
    ExitDirEnum dirBtoA = ExitDirEnum::Invalid; // Placeholder, get_opposite_direction is in RoomSorter
    // Determine opposite direction manually for helper
    if (dirAtoB == ExitDirEnum::North) dirBtoA = ExitDirEnum::South;
    else if (dirAtoB == ExitDirEnum::South) dirBtoA = ExitDirEnum::North;
    else if (dirAtoB == ExitDirEnum::East) dirBtoA = ExitDirEnum::West;
    else if (dirAtoB == ExitDirEnum::West) dirBtoA = ExitDirEnum::East;
    else if (dirAtoB == ExitDirEnum::Up) dirBtoA = ExitDirEnum::Down;
    else if (dirAtoB == ExitDirEnum::Down) dirBtoA = ExitDirEnum::Up;

    if (dirBtoA != ExitDirEnum::Invalid) {
        roomA.getExit(dirAtoB).getOutgoingSet().insert(roomB.getId());
        roomA.getExit(dirAtoB).getExitFields().setExitFlags(ExitFlags(ExitFlag::EXIT_IS_EXIT)); // Mark as actual exit

        roomB.getExit(dirBtoA).getOutgoingSet().insert(roomA.getId());
        roomB.getExit(dirBtoA).getExitFields().setExitFlags(ExitFlags(ExitFlag::EXIT_IS_EXIT));
    }
}


class TestRoomSorter : public QObject {
    Q_OBJECT

private slots:
    void testEmptySelection();
    void testSingleRoomSelection();
    void testSimpleCardinalBlock_NoMove();
    void testAnchoredLooseRoom_Simple();
    void testBlockAndAnchoredRoom();
    void testPlacement_TargetOccupied();

private:
    // Helper to find a room by ID in a vector of RawRooms
    const RawRoom* findRoomInVector(RoomId id, const std::vector<RawRoom>& rooms) {
        for (const auto& room : rooms) {
            if (room.getId() == id) {
                return &room;
            }
        }
        return nullptr;
    }
};

void TestRoomSorter::testEmptySelection() {
    RoomSorter sorter;
    std::vector<RawRoom> selectedRooms;
    std::vector<RawRoom> allMapRooms; // Empty for this test is fine
    std::vector<RawRoom> result = sorter.arrangeRooms(selectedRooms, allMapRooms);
    QVERIFY(result.empty());
}

void TestRoomSorter::testSingleRoomSelection() {
    RoomSorter sorter;
    std::vector<RawRoom> selectedRooms;
    selectedRooms.push_back(createTestRoom(RoomId{1}, Coordinate{0, 0, 0}));
    std::vector<RawRoom> allMapRooms = selectedRooms;

    std::vector<RawRoom> result = sorter.arrangeRooms(selectedRooms, allMapRooms);
    QCOMPARE(result.size(), 1u);
    if (!result.empty()) {
        QCOMPARE(result[0].getPosition(), Coordinate(0, 0, 0)); // Single room shouldn't move
    }
}

void TestRoomSorter::testSimpleCardinalBlock_NoMove() {
    RoomSorter sorter;
    std::vector<RawRoom> selectedRooms;
    RawRoom room1 = createTestRoom(RoomId{1}, Coordinate{0, 0, 0});
    RawRoom room2 = createTestRoom(RoomId{2}, Coordinate{0, 1, 0}); // North of room1
    RawRoom room3 = createTestRoom(RoomId{3}, Coordinate{0, 2, 0}); // North of room2

    addBidirectionalExit(room1, room2, ExitDirEnum::North);
    addBidirectionalExit(room2, room3, ExitDirEnum::North);

    selectedRooms.push_back(room1);
    selectedRooms.push_back(room2);
    selectedRooms.push_back(room3);
    std::vector<RawRoom> allMapRooms = selectedRooms;

    std::vector<RawRoom> result = sorter.arrangeRooms(selectedRooms, allMapRooms);
    QCOMPARE(result.size(), 3u);

    const RawRoom* r1_res = findRoomInVector(RoomId{1}, result);
    const RawRoom* r2_res = findRoomInVector(RoomId{2}, result);
    const RawRoom* r3_res = findRoomInVector(RoomId{3}, result);

    QVERIFY(r1_res && r2_res && r3_res);
    if (r1_res && r2_res && r3_res) {
        // Expect no change in position as they form a perfect cardinal block and no other constraints
        QCOMPARE(r1_res->getPosition(), Coordinate(0, 0, 0));
        QCOMPARE(r2_res->getPosition(), Coordinate(0, 1, 0));
        QCOMPARE(r3_res->getPosition(), Coordinate(0, 2, 0));
    }
}

void TestRoomSorter::testAnchoredLooseRoom_Simple() {
    RoomSorter sorter;
    std::vector<RawRoom> selectedRooms;
    std::vector<RawRoom> allMapRooms;

    RawRoom selectedRoom = createTestRoom(RoomId{1}, Coordinate{5, 5, 0}); // A loose room
    RawRoom anchorRoom = createTestRoom(RoomId{10}, Coordinate{0, 0, 0});   // An unselected anchor

    // Make selectedRoom have a North exit to anchorRoom
    // And anchorRoom have a South exit back to selectedRoom (Strongly Cardinal)
    // For this, selectedRoom should be South of anchorRoom if connected North from selected.
    // Let's adjust positions for clarity:
    // Anchor at (0,2,0). Selected at (0,0,0), connected North to Anchor.
    // Sorter should place selectedRoom at (0,-2,0) (Anchor -> Space -> Selected)

    selectedRoom.setPosition(Coordinate(0,0,0));
    anchorRoom.setPosition(Coordinate(0,2,0)); // Anchor is North of selected's original spot

    // Add exits: selectedRoom (North) <-> anchorRoom (South)
    addBidirectionalExit(selectedRoom, anchorRoom, ExitDirEnum::North);

    selectedRooms.push_back(selectedRoom);
    allMapRooms.push_back(selectedRoom);
    allMapRooms.push_back(anchorRoom); // Anchor is part of allMapRooms but not selectedRooms

    std::vector<RawRoom> result = sorter.arrangeRooms(selectedRooms, allMapRooms);
    QCOMPARE(result.size(), 1u);

    const RawRoom* sel_res = findRoomInVector(RoomId{1}, result);
    QVERIFY(sel_res);
    if (sel_res) {
        // Anchor is at (0,2,0). Selected room connected North to it.
        // AnchorConnection dirFromSelectedToAnchor is North.
        // Opposite is South. Target for selected is 2 units South of Anchor.
        // (0,2,0) - (0,2,0) = (0,0,0) -> This is where the selected room should be to maintain the link with a space
        // No, it should be: anchor (0,2,0) -> space (0,1,0) -> selected (0,0,0)
        // The `cardinalize_groups` places it: get_target_coordinate(anchorPos, dirFromAnchorToNode, 2)
        // anchorPos = (0,2,0), dirFromAnchorToNode = South.
        // So, (0,2,0) + 2 units South = (0,2,0) + (0,-2,0) = (0,0,0)
        // This means the selected room should end up at (0,0,0)
        QCOMPARE(sel_res->getPosition(), Coordinate(0, 0, 0));
    }
}

void TestRoomSorter::testBlockAndAnchoredRoom() {
    RoomSorter sorter;
    std::vector<RawRoom> selectedRooms;
    std::vector<RawRoom> allMapRooms;

    // Cardinal Block R1-R2
    RawRoom r1_block = createTestRoom(RoomId{1}, Coordinate{0, 0, 0});
    RawRoom r2_block = createTestRoom(RoomId{2}, Coordinate{1, 0, 0}); // East of r1
    addBidirectionalExit(r1_block, r2_block, ExitDirEnum::East);
    selectedRooms.push_back(r1_block);
    selectedRooms.push_back(r2_block);

    // Anchored Loose Room R3
    RawRoom r3_loose_selected = createTestRoom(RoomId{3}, Coordinate{10, 10, 0}); // Original position far away
    RawRoom r4_anchor = createTestRoom(RoomId{4}, Coordinate{5, 0, 0}); // Anchor room
    // r3 is South of r4_anchor, connection is North from r3 to r4.
    // r4_anchor is at (5,0,0). r3 should be placed at (5,-2,0) by anchor logic.
    // For the test, we set its original position to a dummy value, RoomSorter should calculate the new one.
    // addBidirectionalExit will make them point to each other.
    addBidirectionalExit(r3_loose_selected, r4_anchor, ExitDirEnum::North);
    selectedRooms.push_back(r3_loose_selected);

    allMapRooms.push_back(r1_block);
    allMapRooms.push_back(r2_block);
    allMapRooms.push_back(r3_loose_selected);
    allMapRooms.push_back(r4_anchor); // Add anchor to allMapRooms

    std::vector<RawRoom> result = sorter.arrangeRooms(selectedRooms, allMapRooms);
    QCOMPARE(result.size(), 3u);

    const RawRoom* r1_res = findRoomInVector(RoomId{1}, result);
    const RawRoom* r2_res = findRoomInVector(RoomId{2}, result);
    const RawRoom* r3_res = findRoomInVector(RoomId{3}, result);
    QVERIFY(r1_res && r2_res && r3_res);

    if (r1_res && r2_res && r3_res) {
        // Block R1-R2 should remain as is, assuming no collision from r3's new position
        // Default placement for blocks is their original position if no other constraints.
        QCOMPARE(r1_res->getPosition(), Coordinate(0, 0, 0));
        QCOMPARE(r2_res->getPosition(), Coordinate(1, 0, 0));

        // R3 should be placed based on its anchor r4_anchor at (5,0,0)
        // R3 connects North to R4. So R3 is South of R4.
        // Expected: R4_anchor (5,0,0) -> Space (5,-1,0) -> R3 (5,-2,0)
        QCOMPARE(r3_res->getPosition(), Coordinate(5, -2, 0));
    }
}

void TestRoomSorter::testPlacement_TargetOccupied() {
    RoomSorter sorter;
    std::vector<RawRoom> selectedRooms;
    std::vector<RawRoom> allMapRooms;

    RawRoom selectedRoomToPlace = createTestRoom(RoomId{1}, Coordinate{0, 0, 0}); // Original position
    RawRoom obstacleRoom = createTestRoom(RoomId{10}, Coordinate{0, 0, 0}); // Occupies target

    selectedRooms.push_back(selectedRoomToPlace);

    allMapRooms.push_back(selectedRoomToPlace);
    allMapRooms.push_back(obstacleRoom);

    std::vector<RawRoom> result = sorter.arrangeRooms(selectedRooms, allMapRooms);
    QCOMPARE(result.size(), 1u);

    const RawRoom* sel_res = findRoomInVector(RoomId{1}, result);
    QVERIFY(sel_res);
    if (sel_res) {
        // Original position (0,0,0) is occupied by obstacleRoom.
        // Expect RoomSorter's find_empty_spot to move it to an adjacent free cardinal spot.
        QVERIFY(sel_res->getPosition() != Coordinate(0,0,0));

        Coordinate diff = sel_res->getPosition() - Coordinate(0,0,0);
        int manhattanDistance = std::abs(diff.x) + std::abs(diff.y) + std::abs(diff.z);
        // The basic find_empty_spot searches dist=1 first.
        QCOMPARE(manhattanDistance, 1);
    }
}


QTEST_APPLESS_MAIN(TestRoomSorter)
#include "TestRoomSorter.moc" // Required for Q_OBJECT if TestRoomSorter is not in a .h file
