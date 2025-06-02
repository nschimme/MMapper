// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include "../../src/mapdata/mapdata.h"
#include "../../src/map/ChangeTypes.h"
#include "../../src/map/Change.h" // Required for ChangeList to work with Change objects
#include "../../src/map/ChangeList.h"
#include "../../src/map/room.h"
#include "../../src/map/coordinate.h"
#include "../../src/map/RawRoom.h" // For AddPermanentRoom potentially, and future RemoveRoom tests.

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <set>
#include <string_view> // Required for RoomArea std::less

// Additional includes for helpers and new tests
#include "../../src/map/RoomFieldVariant.h" // For RoomFieldVariant, FlagModifyModeEnum
#include "../../src/map/ExitDirection.h"   // For ExitDirEnum
// WaysEnum and ChangeTypeEnum are in ChangeTypes.h or Map.h, already covered by existing includes.


// Custom std::less specialization for RoomArea
// Needs to be in the global namespace or std namespace before first use by std::set
namespace std {
    template <>
    struct less<RoomArea> {
        bool operator()(const RoomArea& lhs, const RoomArea& rhs) const {
            return lhs.getStdStringViewUtf8() < rhs.getStdStringViewUtf8();
        }
    };
} // namespace std

namespace { // Anonymous namespace for helper functions

// Static helper function
RoomId addRoomWithArea(MapData& mapData, const Coordinate& pos, const RoomArea& area) {
    ChangeList setupChangesAdd;
    // AddPermanentRoom only takes position. Area must be set subsequently.
    setupChangesAdd.add(Change{room_change_types::AddPermanentRoom{pos}});
    mapData.applyChanges(setupChangesAdd);

    RoomHandle room = mapData.findRoomByPosition(pos); // findRoomByPosition is better after AddPermanentRoom
    if (!room.isValid()) { // Check validity with isValid()
        qWarning("Helper addRoomWithArea: Failed to find room at %s after adding.", qPrintable(pos.toString()));
        return INVALID_ROOMID;
    }
    RoomId roomId = room.getId();

    ChangeList setupChangesArea;
    // RoomFieldVariant can take RoomArea directly if constructor exists, or use appropriate field
    setupChangesArea.add(Change{room_change_types::ModifyRoomFlags{roomId, RoomFieldVariant{area}, FlagModifyModeEnum::ASSIGN}});
    mapData.applyChanges(setupChangesArea);
    
    RoomHandle updatedRoom = mapData.findRoomHandle(roomId);
    if (!updatedRoom.isValid() || updatedRoom.getArea() != area) {
        qWarning("Helper addRoomWithArea: Failed to set area for room %lld at %s. Current area: '%s', Expected: '%s'", 
                 static_cast<long long>(roomId), qPrintable(pos.toString()), 
                 updatedRoom.isValid() ? qPrintable(QString::fromStdString(updatedRoom.getArea().getStdString())) : "N/A", 
                 qPrintable(QString::fromStdString(area.getStdString())) );
        // QVERIFY or QFAIL could be used here if this helper is critical for test setup
    }
    return roomId;
}

// Static helper function
void addExitBetweenRooms(MapData& mapData, RoomId fromRoomId, RoomId toRoomId, ExitDirEnum fromDir, bool twoWay = false) {
    if (fromRoomId == INVALID_ROOMID || toRoomId == INVALID_ROOMID) {
        qWarning("Helper addExitBetweenRooms: Invalid room ID provided. From: %lld, To: %lld", static_cast<long long>(fromRoomId), static_cast<long long>(toRoomId));
        return;
    }
    ChangeList changes;
    changes.add(Change{exit_change_types::ModifyExitConnection{
        ChangeTypeEnum::Add, fromRoomId, fromDir, toRoomId, twoWay ? WaysEnum::TwoWay : WaysEnum::OneWay
    }});
    mapData.applyChanges(changes);
}

} // anonymous namespace

class TestMapDataApplyChangesAreas : public QObject
{
    Q_OBJECT

public:
    TestMapDataApplyChangesAreas() = default;
    ~TestMapDataApplyChangesAreas() = default;

private slots:
    void initTestCase();
    void cleanupTestCase();
    void testWorldChangeGlobalRemesh();
    void testAddRoomSimple();
    void testRemoveRoomGlobalRemesh();
    void testAddExit();
    void testChangeRoomAreaProperty();
    void testChangeOtherRoomProperty();
    void testRemoveExit();
    void testMultipleDistinctChanges();
    void testRoomPropertyChangeThenConnectedRoomChange();
};

void TestMapDataApplyChangesAreas::initTestCase()
{
    // For QSignalSpy arguments
    qRegisterMetaType<std::set<RoomArea>>("std::set<RoomArea>");
    // qRegisterMetaType<RoomArea>("RoomArea"); // Might be needed if RoomArea is used directly in signals (not as set)
}

void TestMapDataApplyChangesAreas::cleanupTestCase()
{
    // Nothing to do here for now
}

void TestMapDataApplyChangesAreas::testWorldChangeGlobalRemesh()
{
    MapData mapData;
    ChangeList changeList;
    changeList.add(Change{world_change_types::GenerateBaseMap{}});

    QSignalSpy spy(&mapData, &MapData::needsAreaRemesh);

    mapData.applyChanges(changeList);

    QCOMPARE(spy.count(), 1); // Check if the signal was emitted once

    QList<QVariant> arguments = spy.takeFirst(); // Get the arguments of the first signal
    QVERIFY(arguments.at(0).canConvert<std::set<RoomArea>>());
    std::set<RoomArea> emittedAreas = arguments.at(0).value<std::set<RoomArea>>();
    
    QVERIFY(emittedAreas.empty()); // For global remesh, the set should be empty
}

void TestMapDataApplyChangesAreas::testAddRoomSimple()
{
    MapData mapData;
    Coordinate newRoomPos(1, 1, 0);

    // Create a dummy RawRoom to satisfy AddPermanentRoom's potential needs if it creates a full room.
    // However, AddPermanentRoom change itself only takes a position.
    // The MapFrontend will handle actual room creation.
    // For this test, we just need to ensure MapData correctly identifies the area of the newly added room.

    ChangeList changes;
    changes.add(Change{room_change_types::AddPermanentRoom{newRoomPos}});

    QSignalSpy spy(&mapData, &MapData::needsAreaRemesh);

    mapData.applyChanges(changes);

    QCOMPARE(spy.count(), 1);

    QList<QVariant> arguments = spy.takeFirst();
    QVERIFY(arguments.at(0).canConvert<std::set<RoomArea>>());
    std::set<RoomArea> affectedAreas = arguments.at(0).value<std::set<RoomArea>>();

    // After changes are applied, the room should exist in mapData
    RoomHandle addedRoomHandle = mapData.findRoomByPosition(newRoomPos); // Use findRoomByPosition
    QVERIFY(addedRoomHandle.isValid()); // Check if the room was actually added and found

    if(addedRoomHandle.isValid()) { // Proceed if room is valid
        std::set<RoomArea> expectedAreas = {addedRoomHandle.getArea()};
        QCOMPARE(affectedAreas.size(), expectedAreas.size());
        QCOMPARE(affectedAreas, expectedAreas);
    } else {
        QFAIL("Added room could not be found in MapData after AddPermanentRoom change.");
    }
}

void TestMapDataApplyChangesAreas::testRemoveRoomGlobalRemesh()
{
    MapData mapData;
    Coordinate roomPos(1, 1, 0);
    RoomId roomIdToRemove = INVALID_ROOMID;

    // 1. Add a room first
    {
        ChangeList addChanges;
        addChanges.add(Change{room_change_types::AddPermanentRoom{roomPos}});
        mapData.applyChanges(addChanges); // Apply directly

        RoomHandle addedRoom = mapData.findRoomByPosition(roomPos);
        QVERIFY(addedRoom.isValid());
        if (!addedRoom.isValid()) {
            QFAIL("Setup for RemoveRoom test failed: Could not add initial room.");
            return;
        }
        roomIdToRemove = addedRoom.getId();
    }
    QVERIFY(roomIdToRemove != INVALID_ROOMID);

    // 2. Now, test removing the room
    ChangeList removeChanges;
    removeChanges.add(Change{room_change_types::RemoveRoom{roomIdToRemove}});

    QSignalSpy spy(&mapData, &MapData::needsAreaRemesh);

    mapData.applyChanges(removeChanges);

    QCOMPARE(spy.count(), 1); // Signal should be emitted

    QList<QVariant> arguments = spy.takeFirst();
    QVERIFY(arguments.at(0).canConvert<std::set<RoomArea>>());
    std::set<RoomArea> emittedAreas = arguments.at(0).value<std::set<RoomArea>>();

    // For RemoveRoom, we now expect a global remesh (empty set)
    // because pre-deletion data isn't available in the change struct itself
    // when MapData::applyChanges processes it.
    QVERIFY(emittedAreas.empty());

    // Verify room is actually gone
    RoomHandle removedRoomHandle = mapData.findRoomHandle(roomIdToRemove);
    QVERIFY(!removedRoomHandle.isValid());
}

void TestMapDataApplyChangesAreas::testAddExit()
{
    MapData mapData;
    RoomArea area1("area1");
    RoomArea area2("area2");

    // Clear any signals from mapData construction or previous interactions if mapData were a member
    // For local mapData, this is not strictly necessary but good practice if helpers emit signals.
    // However, our helpers applyChanges which *will* cause signals.
    // We are interested in the signal from the specific action (addExitBetweenRooms).

    // Setup: Add two rooms with distinct areas
    // We need to ignore signals emitted by these setup steps.
    // One way: disconnect/reconnect, or clear spy before action.
    // Simpler: create spy just before the action.

    RoomId roomId1 = addRoomWithArea(mapData, Coordinate(1,1,0), area1);
    RoomId roomId2 = addRoomWithArea(mapData, Coordinate(2,1,0), area2);
    
    QVERIFY(roomId1 != INVALID_ROOMID);
    QVERIFY(roomId2 != INVALID_ROOMID);
    if (roomId1 == INVALID_ROOMID || roomId2 == INVALID_ROOMID) {
        QFAIL("Failed to setup rooms for testAddExit.");
        return;
    }

    QSignalSpy spy(&mapData, &MapData::needsAreaRemesh); // Create spy AFTER setup and BEFORE action

    addExitBetweenRooms(mapData, roomId1, roomId2, ExitDirEnum::EAST); // Action

    QCOMPARE(spy.count(), 1); // Expect one signal from adding the exit

    QList<QVariant> arguments = spy.takeFirst();
    QVERIFY(arguments.at(0).canConvert<std::set<RoomArea>>());
    std::set<RoomArea> affectedAreas = arguments.at(0).value<std::set<RoomArea>>();
    
    std::set<RoomArea> expectedAreas = {area1, area2};
    QCOMPARE(affectedAreas, expectedAreas);
}

void TestMapDataApplyChangesAreas::testChangeRoomAreaProperty()
{
    MapData mapData;
    RoomArea oldArea("old_area");
    RoomArea connectedArea("connected_area");
    RoomArea newArea("new_area");

    RoomId roomId1 = addRoomWithArea(mapData, Coordinate(1,1,0), oldArea);
    RoomId roomId2 = addRoomWithArea(mapData, Coordinate(2,1,0), connectedArea);
    QVERIFY(roomId1 != INVALID_ROOMID && roomId2 != INVALID_ROOMID);
    addExitBetweenRooms(mapData, roomId1, roomId2, ExitDirEnum::EAST);

    QSignalSpy spy(&mapData, &MapData::needsAreaRemesh);

    ChangeList changes;
    // When RoomArea property is changed, the new area value is used.
    changes.add(Change{room_change_types::ModifyRoomFlags{roomId1, RoomFieldVariant{newArea}, FlagModifyModeEnum::ASSIGN}});
    mapData.applyChanges(changes);

    QCOMPARE(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    QVERIFY(arguments.at(0).canConvert<std::set<RoomArea>>());
    std::set<RoomArea> affectedAreas = arguments.at(0).value<std::set<RoomArea>>();

    // The room itself (with its new area) and directly connected rooms are expected.
    // The oldArea is not expected because the room no longer has that area.
    std::set<RoomArea> expectedAreas = {newArea, connectedArea};
    QCOMPARE(affectedAreas, expectedAreas);

    // Verify the room actually has the new area
    RoomHandle room1Handle = mapData.findRoomHandle(roomId1);
    QVERIFY(room1Handle.isValid());
    QCOMPARE(room1Handle.getArea(), newArea);
}

void TestMapDataApplyChangesAreas::testChangeOtherRoomProperty()
{
    MapData mapData;
    RoomArea area1("area1");
    RoomArea area2("area2");

    RoomId roomId1 = addRoomWithArea(mapData, Coordinate(1,1,0), area1);
    RoomId roomId2 = addRoomWithArea(mapData, Coordinate(2,1,0), area2);
    QVERIFY(roomId1 != INVALID_ROOMID && roomId2 != INVALID_ROOMID);
    addExitBetweenRooms(mapData, roomId1, roomId2, ExitDirEnum::EAST);

    QSignalSpy spy(&mapData, &MapData::needsAreaRemesh);

    ChangeList changes;
    changes.add(Change{room_change_types::ModifyRoomFlags{roomId1, RoomFieldVariant{RoomName("new_name")}, FlagModifyModeEnum::ASSIGN}});
    mapData.applyChanges(changes);

    QCOMPARE(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    QVERIFY(arguments.at(0).canConvert<std::set<RoomArea>>());
    std::set<RoomArea> affectedAreas = arguments.at(0).value<std::set<RoomArea>>();

    // Changing a non-area property should still mark the room and its connected rooms.
    std::set<RoomArea> expectedAreas = {area1, area2};
    QCOMPARE(affectedAreas, expectedAreas);
}

void TestMapDataApplyChangesAreas::testRemoveExit()
{
    MapData mapData;
    RoomArea area1("area1");
    RoomArea area2("area2");

    RoomId roomId1 = addRoomWithArea(mapData, Coordinate(1,1,0), area1);
    RoomId roomId2 = addRoomWithArea(mapData, Coordinate(2,1,0), area2);
    QVERIFY(roomId1 != INVALID_ROOMID && roomId2 != INVALID_ROOMID);
    addExitBetweenRooms(mapData, roomId1, roomId2, ExitDirEnum::EAST, true); // two-way

    QSignalSpy spy(&mapData, &MapData::needsAreaRemesh);

    ChangeList changes;
    // Remove the exit connection
    changes.add(Change{exit_change_types::ModifyExitConnection{ChangeTypeEnum::Remove, roomId1, ExitDirEnum::EAST, roomId2, WaysEnum::TwoWay}});
    mapData.applyChanges(changes);

    QCOMPARE(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    QVERIFY(arguments.at(0).canConvert<std::set<RoomArea>>());
    std::set<RoomArea> affectedAreas = arguments.at(0).value<std::set<RoomArea>>();
    
    std::set<RoomArea> expectedAreas = {area1, area2};
    QCOMPARE(affectedAreas, expectedAreas);
}

void TestMapDataApplyChangesAreas::testMultipleDistinctChanges()
{
    MapData mapData;
    RoomArea areaR1("area_r1");
    RoomArea areaR2("area_r2"); // Unconnected to R1's change, should not appear
    RoomArea areaR3("area_r3");
    RoomArea areaR4("area_r4");

    RoomId r1 = addRoomWithArea(mapData, Coordinate(1,1,0), areaR1);
    /*RoomId r2 =*/ addRoomWithArea(mapData, Coordinate(2,1,0), areaR2); // r2 is created but not involved in changes that link to other areas being tested
    RoomId r3 = addRoomWithArea(mapData, Coordinate(3,1,0), areaR3);
    RoomId r4 = addRoomWithArea(mapData, Coordinate(4,1,0), areaR4);
    QVERIFY(r1 != INVALID_ROOMID && r3 != INVALID_ROOMID && r4 != INVALID_ROOMID);

    addExitBetweenRooms(mapData, r3, r4, ExitDirEnum::NORTH);

    QSignalSpy spy(&mapData, &MapData::needsAreaRemesh);

    ChangeList changes;
    changes.add(Change{room_change_types::ModifyRoomFlags{r1, RoomFieldVariant{RoomName("name1")}, FlagModifyModeEnum::ASSIGN}}); // Affects r1 (areaR1)
    changes.add(Change{exit_change_types::ModifyExitConnection{ChangeTypeEnum::Remove, r3, ExitDirEnum::NORTH, r4, WaysEnum::OneWay}}); // Affects r3 (areaR3), r4 (areaR4)
    mapData.applyChanges(changes);

    QCOMPARE(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    QVERIFY(arguments.at(0).canConvert<std::set<RoomArea>>());
    std::set<RoomArea> affectedAreas = arguments.at(0).value<std::set<RoomArea>>();
    
    std::set<RoomArea> expectedAreas = {areaR1, areaR3, areaR4};
    QCOMPARE(affectedAreas, expectedAreas);
}

void TestMapDataApplyChangesAreas::testRoomPropertyChangeThenConnectedRoomChange()
{
    MapData mapData;
    RoomArea areaA("areaA");
    RoomArea areaB("areaB");
    RoomArea areaC("areaC");

    RoomId rA = addRoomWithArea(mapData, Coordinate(1,0,0), areaA);
    RoomId rB = addRoomWithArea(mapData, Coordinate(2,0,0), areaB);
    RoomId rC = addRoomWithArea(mapData, Coordinate(3,0,0), areaC);
    QVERIFY(rA != INVALID_ROOMID && rB != INVALID_ROOMID && rC != INVALID_ROOMID);

    addExitBetweenRooms(mapData, rA, rB, ExitDirEnum::EAST);
    addExitBetweenRooms(mapData, rB, rC, ExitDirEnum::EAST);

    QSignalSpy spy(&mapData, &MapData::needsAreaRemesh);

    ChangeList changes;
    // Change to rA should mark areaA. Connected areaB is also added.
    changes.add(Change{room_change_types::ModifyRoomFlags{rA, RoomFieldVariant{RoomName("nameA")}, FlagModifyModeEnum::ASSIGN}});
    // Change to rC should mark areaC. Connected areaB is also added.
    // Since areaB is already included from rA's change, the set union naturally handles it.
    changes.add(Change{room_change_types::ModifyRoomFlags{rC, RoomFieldVariant{RoomName("nameC")}, FlagModifyModeEnum::ASSIGN}});
    mapData.applyChanges(changes);

    QCOMPARE(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    QVERIFY(arguments.at(0).canConvert<std::set<RoomArea>>());
    std::set<RoomArea> affectedAreas = arguments.at(0).value<std::set<RoomArea>>();
    
    std::set<RoomArea> expectedAreas = {areaA, areaB, areaC};
    QCOMPARE(affectedAreas, expectedAreas);
}


QTEST_MAIN(TestMapDataApplyChangesAreas)
#include "test_mapdata_applychanges_areas.moc"
