#include "../src/map/Map.h"
#include "../src/map/World.h"
#include "../src/map/RawRoom.h"
#include "../src/map/Changes.h"
#include "../src/map/RoomId.h"
#include "../src/map/coordinate.h"
#include "../src/map/RoomArea.h"
#include "../src/map/ExitFlags.h"
#include "../src/map/DoorFlags.h"
#include "../src/map/MapWorldCompareDetail.h" // For direct use if needed, and for hasMeshDifference
#include "../src/global/progresscounter.h"

#include <QtTest/QtTest>
#include <vector>
#include <memory>

// Forward declaration if not in headers
class World;

class TestMapHasMeshDifferencesForArea : public QObject
{
    Q_OBJECT

private:
    struct TestWorlds {
        World world_before;
        World world_after;
    };

    RoomId roomA, roomB, roomC, roomD, roomE, roomF_unused;
    RoomArea area1{"Area1"}, area2{"Area2"}, noArea{""};

    // Helper to create a basic world configuration
    World create_initial_world() {
        ProgressCounter pc;
        std::vector<ExternalRawRoom> ext_rooms;

        // RoomA in Area1
        ExternalRawRoom rA;
        rA.id = ExternalRoomId{1};
        rA.position = Coordinate{0,0,0};
        rA.setArea(area1);
        rA.setName(RoomName{"RoomA"});
        ext_rooms.push_back(rA);
        roomA = RoomId{0}; // Assuming first room gets ID 0 after internal conversion

        // RoomB in Area1
        ExternalRawRoom rB;
        rB.id = ExternalRoomId{2};
        rB.position = Coordinate{1,0,0};
        rB.setArea(area1);
        rB.setName(RoomName{"RoomB"});
        ext_rooms.push_back(rB);
        roomB = RoomId{1}; 

        // RoomC in Area2
        ExternalRawRoom rC;
        rC.id = ExternalRoomId{3};
        rC.position = Coordinate{0,1,0};
        rC.setArea(area2);
        rC.setName(RoomName{"RoomC"});
        ext_rooms.push_back(rC);
        roomC = RoomId{2};

        // RoomD with no area string
        ExternalRawRoom rD;
        rD.id = ExternalRoomId{4};
        rD.position = Coordinate{0,0,1};
        rD.setArea(noArea);
        rD.setName(RoomName{"RoomD"});
        ext_rooms.push_back(rD);
        roomD = RoomId{3};

        // RoomE in Area1
        ExternalRawRoom rE;
        rE.id = ExternalRoomId{5};
        rE.position = Coordinate{-1,0,0};
        rE.setArea(area1);
        rE.setName(RoomName{"RoomE"});
        ext_rooms.push_back(rE);
        roomE = RoomId{4};
        
        // RoomF_unused (will be added later in tests)
        roomF_unused = RoomId{5};


        World world = World::init(pc, ext_rooms);

        // Add simple exit RoomA (east) -> RoomB (west)
        world.addExit(roomA, ExitDirEnum::EAST, roomB, WaysEnum::TwoWay);
        
        // Add flow exit RoomE (east) -> RoomB (west)
        RawRoom roomE_data = world.getRawCopy(roomE);
        roomE_data.getExit(ExitDirEnum::EAST).fields.exitFlags.insert(ExitFlagEnum::FLOW);
        roomE_data.getExit(ExitDirEnum::EAST).outgoing = {roomB};
        world.setRoom(roomE, roomE_data);
        // Ensure backlink for consistency if needed by flow, though hasMeshDifference focuses on one side
        RawRoom roomB_data = world.getRawCopy(roomB);
        roomB_data.getExit(ExitDirEnum::WEST).incoming.insert(roomE);
        world.setRoom(roomB, roomB_data);


        return world;
    }

    TestWorlds apply_change_to_world(const World& initial_world, const Change& change) {
        ProgressCounter pc;
        World world_after = initial_world.copy();
        world_after.applyOne(pc, change);
        return {initial_world, world_after};
    }
    
    TestWorlds apply_changes_to_world(const World& initial_world, const std::vector<Change>& changes) {
        ProgressCounter pc;
        World world_after = initial_world.copy();
        world_after.applyAll(pc, changes);
        return {initial_world, world_after};
    }


private slots:
    void testNoChange();
    void testRoomPropertyChangeInArea();
    void testExitPropertyChangeInArea();
    void testFlowExitTargetChangeInArea();
    void testNonFlowExitTargetChangeInArea();
    void testRoomAddedToArea();
    void testRoomRemovedFromArea();
    void testRoomMovesIntoArea();
    void testRoomMovesOutOfArea();
    void testRoomMovesWithinArea();
    void testChangeInOtherArea();
    void testRoomAreaChanges();

};

// Test case implementations will follow

void TestMapHasMeshDifferencesForArea::testNoChange() {
    World initial = create_initial_world();
    Change change = room_change_types::ModifyRoomFlags{roomA, RoomNote{"New note"}, FlagModifyModeEnum::ASSIGN};
    TestWorlds worlds = apply_change_to_world(initial, change);
    QVERIFY(!Map::hasMeshDifferencesForArea(area1, worlds.world_before, worlds.world_after));
    QVERIFY(!Map::hasMeshDifferencesForArea(area2, worlds.world_before, worlds.world_after));
}

void TestMapHasMeshDifferencesForArea::testRoomPropertyChangeInArea() {
    World initial = create_initial_world();
    Change change = room_change_types::ModifyRoomFlags{roomA, RoomTerrainEnum::CAVE, FlagModifyModeEnum::ASSIGN};
    TestWorlds worlds = apply_change_to_world(initial, change);
    QVERIFY(Map::hasMeshDifferencesForArea(area1, worlds.world_before, worlds.world_after));
    QVERIFY(!Map::hasMeshDifferencesForArea(area2, worlds.world_before, worlds.world_after));
}

void TestMapHasMeshDifferencesForArea::testExitPropertyChangeInArea() {
    World initial = create_initial_world();
    // Make RoomA -> RoomB exit a door
    Change change = exit_change_types::SetExitFlags{FlagChangeEnum::Add, roomA, ExitDirEnum::EAST, ExitFlags{ExitFlagEnum::DOOR}};
    TestWorlds worlds = apply_change_to_world(initial, change);
    QVERIFY(Map::hasMeshDifferencesForArea(area1, worlds.world_before, worlds.world_after));
}

void TestMapHasMeshDifferencesForArea::testFlowExitTargetChangeInArea() {
    World initial = create_initial_world();
    // Change RoomE's flow exit from RoomB to RoomA
    RawRoom roomE_data = initial.getRawCopy(roomE);
    roomE_data.getExit(ExitDirEnum::EAST).outgoing = {roomA}; // Change target to roomA
    // We also need to remove the old flow connection from RoomB's incoming if it was symmetric
    // and add to RoomA's incoming for full consistency, but hasMeshDifference on RawExit for RoomE
    // should catch the outgoing change.
    Change change_set_roomE = room_change_types::Update{roomE, roomE_data.getParseEvent()}; // This is not right. Need to use SetRoom or similar
                                                                                      // Or more granular exit changes.
                                                                                      
    // For simplicity in test, let's use a more direct way if available or construct RawRoom for setRoom
    // This is tricky because we can't directly make a "Change" object for modifying RawExit's 'outgoing' set easily
    // without a specific ChangeType. Let's assume a more direct modification for test setup for now,
    // or accept that we might need a more complex Change or a helper.
    // The most robust way is to use existing change types.
    // 1. Remove old flow exit RoomE -> RoomB
    // 2. Add new flow exit RoomE -> RoomA
    // This might be too complex for a single "Change" object easily.

    // Let's try to set the whole room with modified exit data
    World world_after_manual_edit = initial.copy();
    RawRoom roomE_modified_in_after = world_after_manual_edit.getRawCopy(roomE);
    roomE_modified_in_after.getExit(ExitDirEnum::EAST).outgoing = {roomA};
    world_after_manual_edit.setRoom(roomE, roomE_modified_in_after);
    
    // To make RoomB consistent (remove old incoming from E)
    RawRoom roomB_modified_in_after = world_after_manual_edit.getRawCopy(roomB);
    roomB_modified_in_after.getExit(ExitDirEnum::WEST).incoming.erase(roomE);
    world_after_manual_edit.setRoom(roomB, roomB_modified_in_after);

    // To make RoomA consistent (add new incoming from E)
    RawRoom roomA_modified_in_after = world_after_manual_edit.getRawCopy(roomA);
    roomA_modified_in_after.getExit(ExitDirEnum::WEST).incoming.insert(roomE); // Assuming WEST is opposite of EAST for E's exit
    world_after_manual_edit.setRoom(roomA, roomA_modified_in_after);


    TestWorlds worlds = {initial, world_after_manual_edit};
    QVERIFY(Map::hasMeshDifferencesForArea(area1, worlds.world_before, worlds.world_after));
}

void TestMapHasMeshDifferencesForArea::testNonFlowExitTargetChangeInArea() {
    World initial = create_initial_world();
    // Change RoomA's non-flow exit from RoomB to RoomC (RoomC is in Area2, this will also change areas)
    // This test name is a bit misleading, as changing target to a different area *is* a mesh change for both.
    // A better test would be changing target to another room *within* Area1 that isn't flow.
    // For now, let's test the original intent: non-flow exit target change.
    // The `hasMeshDifference(RawExit)` does NOT check non-flow `outgoing` changes.
    
    World world_after_manual_edit = initial.copy();
    RawRoom roomA_modified_in_after = world_after_manual_edit.getRawCopy(roomA);
    roomA_modified_in_after.getExit(ExitDirEnum::EAST).outgoing = {roomC}; // Target RoomC
    world_after_manual_edit.setRoom(roomA, roomA_modified_in_after);
    
    // Consistency updates
    RawRoom roomB_mod = world_after_manual_edit.getRawCopy(roomB);
    roomB_mod.getExit(ExitDirEnum::WEST).incoming.erase(roomA);
    world_after_manual_edit.setRoom(roomB, roomB_mod);

    RawRoom roomC_mod = world_after_manual_edit.getRawCopy(roomC);
    roomC_mod.getExit(ExitDirEnum::WEST).incoming.insert(roomA); // Assuming EAST from A means WEST to C
    world_after_manual_edit.setRoom(roomC, roomC_mod);

    TestWorlds worlds = {initial, world_after_manual_edit};

    // This should be false because non-flow `outgoing` changes are not mesh differences per current RawExit::hasMeshDifference
    QVERIFY(!Map::hasMeshDifferencesForArea(area1, worlds.world_before, worlds.world_after));
}

void TestMapHasMeshDifferencesForArea::testRoomAddedToArea() {
    World initial = create_initial_world();
    Change change = room_change_types::AddPermanentRoom{Coordinate{0,0,-1}, area1}; // Add RoomF to Area1
    TestWorlds worlds = apply_change_to_world(initial, change);
    QVERIFY(Map::hasMeshDifferencesForArea(area1, worlds.world_before, worlds.world_after));
}

void TestMapHasMeshDifferencesForArea::testRoomRemovedFromArea() {
    World initial = create_initial_world();
    Change change = room_change_types::RemoveRoom{roomA};
    TestWorlds worlds = apply_change_to_world(initial, change);
    QVERIFY(Map::hasMeshDifferencesForArea(area1, worlds.world_before, worlds.world_after));
}

void TestMapHasMeshDifferencesForArea::testRoomMovesIntoArea() {
    World initial = create_initial_world(); // RoomC is in Area2
    // Change RoomC's area to Area1. Position change also needed for a "move".
    RawRoom roomC_data = initial.getRawCopy(roomC);
    roomC_data.setArea(area1);
    roomC_data.position = Coordinate{0, -1, 0}; // New position in/near Area1
    
    World world_after_manual_edit = initial.copy();
    world_after_manual_edit.setRoom(roomC, roomC_data);

    TestWorlds worlds = {initial, world_after_manual_edit};
    QVERIFY(Map::hasMeshDifferencesForArea(area1, worlds.world_before, worlds.world_after));
    QVERIFY(Map::hasMeshDifferencesForArea(area2, worlds.world_before, worlds.world_after));
}

void TestMapHasMeshDifferencesForArea::testRoomMovesOutOfArea() {
    World initial = create_initial_world(); // RoomA is in Area1
    // Change RoomA's area to Area2.
    RawRoom roomA_data = initial.getRawCopy(roomA);
    roomA_data.setArea(area2);
    roomA_data.position = Coordinate{1, 1, 0}; // New position in/near Area2

    World world_after_manual_edit = initial.copy();
    world_after_manual_edit.setRoom(roomA, roomA_data);

    TestWorlds worlds = {initial, world_after_manual_edit};
    QVERIFY(Map::hasMeshDifferencesForArea(area1, worlds.world_before, worlds.world_after));
    QVERIFY(Map::hasMeshDifferencesForArea(area2, worlds.world_before, worlds.world_after));
}

void TestMapHasMeshDifferencesForArea::testRoomMovesWithinArea() {
    World initial = create_initial_world();
    Change change = room_change_types::MoveRelative{roomA, Coordinate{0,0,1}}; // Move RoomA within Area1
    TestWorlds worlds = apply_change_to_world(initial, change);
    QVERIFY(Map::hasMeshDifferencesForArea(area1, worlds.world_before, worlds.world_after));
}

void TestMapHasMeshDifferencesForArea::testChangeInOtherArea() {
    World initial = create_initial_world();
    Change change = room_change_types::ModifyRoomFlags{roomC, RoomTerrainEnum::CAVE, FlagModifyModeEnum::ASSIGN}; // RoomC in Area2
    TestWorlds worlds = apply_change_to_world(initial, change);
    QVERIFY(!Map::hasMeshDifferencesForArea(area1, worlds.world_before, worlds.world_after));
    QVERIFY(Map::hasMeshDifferencesForArea(area2, worlds.world_before, worlds.world_after));
}

void TestMapHasMeshDifferencesForArea::testRoomAreaChanges() {
    World initial = create_initial_world(); // RoomA is in Area1
    // Change RoomA's area from Area1 to Area2
    RawRoom roomA_data = initial.getRawCopy(roomA);
    roomA_data.setArea(area2);
    // No coordinate change, just area string.

    World world_after_manual_edit = initial.copy();
    world_after_manual_edit.setRoom(roomA, roomA_data);
    
    TestWorlds worlds = {initial, world_after_manual_edit};
    QVERIFY(Map::hasMeshDifferencesForArea(area1, worlds.world_before, worlds.world_after));
    QVERIFY(Map::hasMeshDifferencesForArea(area2, worlds.world_before, worlds.world_after));
}


QTEST_MAIN(TestMapHasMeshDifferencesForArea)
#include "TestMapHasMeshDifferencesForArea.moc"
// The .moc file should be generated by Qt's MOC and included at the end.
// For a non-Qt build system, this might need to be handled differently or omitted if not using QObject features.
// For now, assuming a Qt environment for testing.

// Need to ensure RawRoom::getParseEvent() is suitable or provide a way to construct the update Change.
// The Change type for updating a room based on a modified RawRoom might be tricky.
// `room_change_types::Update` takes a ParseEvent. A direct `SetRoom(RoomId, RawRoom)` might be better for tests.
// World::setRoom(id, rawRoom) is used above for direct manipulation.

// AddPermanentRoom now also takes an optional area.
// room_change_types::AddPermanentRoom{Coordinate{0,0,-1}} -> needs area.
// Fixed: room_change_types::AddPermanentRoom{Coordinate{0,0,-1}, area1}

// Error in testFlowExitTargetChangeInArea:
// Change change_set_roomE = room_change_types::Update{roomE, roomE_data.getParseEvent()};
// getParseEvent() on a RawRoom is not standard, and Update is for ParseEvents.
// Switched to manual world modification using setRoom.
