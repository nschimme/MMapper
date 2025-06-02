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
#include <set> // Required for std::set in WorldComparisonStats

// Forward declaration if not in headers
// class World; // Already included via World.h

class TestWorldComparisonStats : public QObject // Renamed class
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
        roomA = RoomId{0}; 

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
        
        roomF_unused = RoomId{5};


        World world = World::init(pc, ext_rooms);

        world.addExit(roomA, ExitDirEnum::EAST, roomB, WaysEnum::TwoWay);
        
        RawRoom roomE_data = world.getRawCopy(roomE);
        roomE_data.getExit(ExitDirEnum::EAST).fields.exitFlags.insert(ExitFlagEnum::FLOW);
        roomE_data.getExit(ExitDirEnum::EAST).outgoing = {roomB};
        world.setRoom(roomE, roomE_data);
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
    void initTestCase() {
        // For QVERIFY/QCOMPARE with std::set<RoomArea> if it's used directly in those macros
        // qRegisterMetaType<std::set<RoomArea>>("std::set<RoomArea>"); // Might not be needed if only checking .count() or .empty()
    }

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

// Test case implementations adapted for WorldComparisonStats
void TestWorldComparisonStats::testNoChange() {
    World initial = create_initial_world();
    Change change = room_change_types::ModifyRoomFlags{roomA, RoomNote{"New note"}, FlagModifyModeEnum::ASSIGN};
    TestWorlds worlds = apply_change_to_world(initial, change);
    WorldComparisonStats stats = World::getComparisonStats(worlds.world_before, worlds.world_after);
    QVERIFY(stats.visuallyDirtyAreas.empty());
    QVERIFY(!stats.hasMeshDifferences); // No mesh diff for note change
}

void TestWorldComparisonStats::testRoomPropertyChangeInArea() {
    World initial = create_initial_world();
    Change change = room_change_types::ModifyRoomFlags{roomA, RoomTerrainEnum::CAVE, FlagModifyModeEnum::ASSIGN};
    TestWorlds worlds = apply_change_to_world(initial, change);
    WorldComparisonStats stats = World::getComparisonStats(worlds.world_before, worlds.world_after);
    QVERIFY(stats.visuallyDirtyAreas.count(area1) == 1);
    QVERIFY(stats.visuallyDirtyAreas.count(area2) == 0);
    QCOMPARE(stats.visuallyDirtyAreas.size(), size_t{1});
    QVERIFY(stats.hasMeshDifferences);
}

void TestWorldComparisonStats::testExitPropertyChangeInArea() {
    World initial = create_initial_world();
    Change change = exit_change_types::SetExitFlags{FlagChangeEnum::Add, roomA, ExitDirEnum::EAST, ExitFlags{ExitFlagEnum::DOOR}};
    TestWorlds worlds = apply_change_to_world(initial, change);
    WorldComparisonStats stats = World::getComparisonStats(worlds.world_before, worlds.world_after);
    QVERIFY(stats.visuallyDirtyAreas.count(area1) == 1);
    QCOMPARE(stats.visuallyDirtyAreas.size(), size_t{1});
    QVERIFY(stats.hasMeshDifferences);
}

void TestWorldComparisonStats::testFlowExitTargetChangeInArea() {
    World initial = create_initial_world();
    World world_after_manual_edit = initial.copy();
    RawRoom roomE_modified_in_after = world_after_manual_edit.getRawCopy(roomE);
    roomE_modified_in_after.getExit(ExitDirEnum::EAST).outgoing = {roomA}; // Target roomA
    world_after_manual_edit.setRoom(roomE, roomE_modified_in_after);
    
    RawRoom roomB_modified_in_after = world_after_manual_edit.getRawCopy(roomB);
    roomB_modified_in_after.getExit(ExitDirEnum::WEST).incoming.erase(roomE);
    world_after_manual_edit.setRoom(roomB, roomB_modified_in_after);

    RawRoom roomA_modified_in_after = world_after_manual_edit.getRawCopy(roomA);
    roomA_modified_in_after.getExit(ExitDirEnum::WEST).incoming.insert(roomE);
    world_after_manual_edit.setRoom(roomA, roomA_modified_in_after);

    TestWorlds worlds = {initial, world_after_manual_edit};
    WorldComparisonStats stats = World::getComparisonStats(worlds.world_before, worlds.world_after);
    QVERIFY(stats.visuallyDirtyAreas.count(area1) == 1);
    QCOMPARE(stats.visuallyDirtyAreas.size(), size_t{1}); // Only Area1 should be dirty
    QVERIFY(stats.hasMeshDifferences);
}

void TestWorldComparisonStats::testNonFlowExitTargetChangeInArea() {
    World initial = create_initial_world();
    World world_after_manual_edit = initial.copy();
    RawRoom roomA_modified_in_after = world_after_manual_edit.getRawCopy(roomA);
    // Change target from RoomB (Area1) to RoomD (NoArea) - this is a non-flow exit
    roomA_modified_in_after.getExit(ExitDirEnum::EAST).outgoing = {roomD}; 
    world_after_manual_edit.setRoom(roomA, roomA_modified_in_after);
    
    RawRoom roomB_mod = world_after_manual_edit.getRawCopy(roomB);
    roomB_mod.getExit(ExitDirEnum::WEST).incoming.erase(roomA);
    world_after_manual_edit.setRoom(roomB, roomB_mod);

    RawRoom roomD_mod = world_after_manual_edit.getRawCopy(roomD);
    roomD_mod.getExit(ExitDirEnum::WEST).incoming.insert(roomA); 
    world_after_manual_edit.setRoom(roomD, roomD_mod);

    TestWorlds worlds = {initial, world_after_manual_edit};
    WorldComparisonStats stats = World::getComparisonStats(worlds.world_before, worlds.world_after);
    // Non-flow outgoing changes are NOT mesh differences according to RawExit::hasMeshDifference
    QVERIFY(stats.visuallyDirtyAreas.empty()); 
    QVERIFY(!stats.hasMeshDifferences);
}

void TestWorldComparisonStats::testRoomAddedToArea() {
    World initial = create_initial_world();
    Change change = room_change_types::AddPermanentRoom{Coordinate{0,0,-1}, area1};
    TestWorlds worlds = apply_change_to_world(initial, change);
    WorldComparisonStats stats = World::getComparisonStats(worlds.world_before, worlds.world_after);
    QVERIFY(stats.visuallyDirtyAreas.count(area1) == 1);
    QCOMPARE(stats.visuallyDirtyAreas.size(), size_t{1});
    QVERIFY(stats.hasMeshDifferences);
    QVERIFY(stats.anyRoomsAdded);
}

void TestWorldComparisonStats::testRoomRemovedFromArea() {
    World initial = create_initial_world();
    Change change = room_change_types::RemoveRoom{roomA}; // roomA is in area1
    TestWorlds worlds = apply_change_to_world(initial, change);
    WorldComparisonStats stats = World::getComparisonStats(worlds.world_before, worlds.world_after);
    QVERIFY(stats.visuallyDirtyAreas.count(area1) == 1);
    QCOMPARE(stats.visuallyDirtyAreas.size(), size_t{1});
    QVERIFY(stats.hasMeshDifferences);
    QVERIFY(stats.anyRoomsRemoved);
}

void TestWorldComparisonStats::testRoomMovesIntoArea() {
    World initial = create_initial_world(); // RoomC is in Area2
    World world_after_manual_edit = initial.copy();
    RawRoom roomC_data = world_after_manual_edit.getRawCopy(roomC);
    roomC_data.setArea(area1); // Moves from Area2 to Area1
    roomC_data.position = Coordinate{0, -1, 0}; 
    world_after_manual_edit.setRoom(roomC, roomC_data);

    TestWorlds worlds = {initial, world_after_manual_edit};
    WorldComparisonStats stats = World::getComparisonStats(worlds.world_before, worlds.world_after);
    QVERIFY(stats.visuallyDirtyAreas.count(area1) == 1);
    QVERIFY(stats.visuallyDirtyAreas.count(area2) == 1);
    QCOMPARE(stats.visuallyDirtyAreas.size(), size_t{2});
    QVERIFY(stats.hasMeshDifferences);
    QVERIFY(stats.spatialDbChanged); // Position changed
}

void TestWorldComparisonStats::testRoomMovesOutOfArea() {
    World initial = create_initial_world(); // RoomA is in Area1
    World world_after_manual_edit = initial.copy();
    RawRoom roomA_data = world_after_manual_edit.getRawCopy(roomA);
    roomA_data.setArea(area2); // Moves from Area1 to Area2
    roomA_data.position = Coordinate{1, 1, 0}; 
    world_after_manual_edit.setRoom(roomA, roomA_data);

    TestWorlds worlds = {initial, world_after_manual_edit};
    WorldComparisonStats stats = World::getComparisonStats(worlds.world_before, worlds.world_after);
    QVERIFY(stats.visuallyDirtyAreas.count(area1) == 1);
    QVERIFY(stats.visuallyDirtyAreas.count(area2) == 1);
    QCOMPARE(stats.visuallyDirtyAreas.size(), size_t{2});
    QVERIFY(stats.hasMeshDifferences);
    QVERIFY(stats.spatialDbChanged);
}

void TestWorldComparisonStats::testRoomMovesWithinArea() {
    World initial = create_initial_world();
    Change change = room_change_types::MoveRelative{roomA, Coordinate{0,0,1}}; // RoomA in Area1
    TestWorlds worlds = apply_change_to_world(initial, change);
    WorldComparisonStats stats = World::getComparisonStats(worlds.world_before, worlds.world_after);
    QVERIFY(stats.visuallyDirtyAreas.count(area1) == 1);
    QCOMPARE(stats.visuallyDirtyAreas.size(), size_t{1});
    QVERIFY(stats.hasMeshDifferences);
    QVERIFY(stats.spatialDbChanged);
}

void TestWorldComparisonStats::testChangeInOtherArea() {
    World initial = create_initial_world();
    Change change = room_change_types::ModifyRoomFlags{roomC, RoomTerrainEnum::CAVE, FlagModifyModeEnum::ASSIGN}; // RoomC in Area2
    TestWorlds worlds = apply_change_to_world(initial, change);
    WorldComparisonStats stats = World::getComparisonStats(worlds.world_before, worlds.world_after);
    QVERIFY(stats.visuallyDirtyAreas.count(area1) == 0);
    QVERIFY(stats.visuallyDirtyAreas.count(area2) == 1);
    QCOMPARE(stats.visuallyDirtyAreas.size(), size_t{1});
    QVERIFY(stats.hasMeshDifferences);
}

void TestWorldComparisonStats::testRoomAreaChanges() {
    World initial = create_initial_world(); // RoomA is in Area1
    World world_after_manual_edit = initial.copy();
    RawRoom roomA_data = world_after_manual_edit.getRawCopy(roomA);
    roomA_data.setArea(area2); // Change area string from Area1 to Area2
    world_after_manual_edit.setRoom(roomA, roomA_data);
    
    TestWorlds worlds = {initial, world_after_manual_edit};
    WorldComparisonStats stats = World::getComparisonStats(worlds.world_before, worlds.world_after);
    QVERIFY(stats.visuallyDirtyAreas.count(area1) == 1); // Old area is dirty (room removed from it)
    QVERIFY(stats.visuallyDirtyAreas.count(area2) == 1); // New area is dirty (room added to it)
    QCOMPARE(stats.visuallyDirtyAreas.size(), size_t{2});
    QVERIFY(stats.hasMeshDifferences); // Because areas changed, implies a structural change
}

QTEST_MAIN(TestWorldComparisonStats)
#include "TestWorldComparisonStats.moc"
