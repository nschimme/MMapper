// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "TestMainWindow.h"

#include "../src/global/Version.h"
#include "../src/mainwindow/UpdateDialog.h"

#include <QDebug>
#include <QtTest/QtTest>

#include "../src/mapdata/mapdata.h"    // For MapData
#include "../src/map/Changes.h"        // For Change and change_types
#include "../src/map/RoomFieldVariant.h" // For RoomName
#include "../src/map/ProgressCounter.h" // For ProgressCounter
#include "../src/map/coordinate.h"    // For Coordinate
#include "../src/map/infomark.h"      // For InfoMarkFields, InfoMarkText
#include "../src/map/MapConsistencyError.h" // For MapConsistencyError (good to have for context)

const char *getMMapperVersion()
{
    return "v19.04.0-72-ga16c196";
}

const char *getMMapperBranch()
{
    return "master";
}

bool isMMapperBeta()
{
    return false;
}

TestMainWindow::TestMainWindow() = default;

TestMainWindow::~TestMainWindow() = default;

void TestMainWindow::updaterTest()
{
    CompareVersion version{QString::fromUtf8(getMMapperVersion())};
    QVERIFY2(version == version, "Version compared to itself matches");

    CompareVersion current{"2.8.0"};
    QVERIFY2((current > current) == false, "Current is not greater than itself");

    CompareVersion newerMajor{"19.04.0"};
    QVERIFY2(newerMajor > current, "Newer major version is greater than older version");
    QVERIFY2((current > newerMajor) == false, "Older version is not newer than newer major");

    CompareVersion newerMinor{"2.9.0"};
    QVERIFY2(newerMinor > current, "Newer major version is greater than older version");
    QVERIFY2((current > newerMinor) == false, "Older version is not newer than newer minor version");

    CompareVersion newerPatch{"2.9.0"};
    QVERIFY2(newerPatch > current, "Newer major version is greater than older version");
    QVERIFY2((current > newerPatch) == false, "Older version is not newer than newer patch version");
}

void TestMainWindow::snapshotTest()
{
    MapData mapData(nullptr); // Parent QObject, null for simple tests

    // Initial state
    Map initialMap = mapData.getCurrentMap();
    InfomarkDb initialMarks = mapData.getCurrentMarks();
    QVERIFY(initialMap.empty());
    QVERIFY(initialMarks.empty());

    // Take snapshot of initial empty state
    mapData.takeSnapshot();
    QCOMPARE(mapData.getSnapshotMap(), initialMap);
    QCOMPARE(mapData.getSnapshotMarks(), initialMarks);

    // --- Modify current map ---
    ProgressCounter pc;
    Coordinate roomCoord(1, 2, 0);
    RoomName testRoomName("Test Room");
    Change addRoomChange{room_change_types::AddPermanentRoom{roomCoord}};
    QVERIFY(mapData.applySingleChange(pc, addRoomChange));

    RoomId newRoomId = mapData.getCurrentMap().findAllRooms(roomCoord).first();
    QVERIFY(newRoomId != INVALID_ROOMID);
    Change setRoomNameChange{room_change_types::SetRoomName{newRoomId, testRoomName}};
    QVERIFY(mapData.applySingleChange(pc, setRoomNameChange));

    // Verify current map changed
    QVERIFY(!mapData.getCurrentMap().empty());
    QCOMPARE(mapData.getCurrentMap().getRoomHandle(newRoomId).getName(), testRoomName);

    // Snapshot should still be the initial empty state
    QCOMPARE(mapData.getSnapshotMap(), initialMap);
    QVERIFY(mapData.getSnapshotMap() != mapData.getCurrentMap());

    // --- Modify current marks ---
    InfoMarkFields markFields;
    markFields.setText(InfoMarkText("Test Mark"));
    markFields.setPosition1(Coordinate(10,10,0)); // Example position
    markFields.setPosition2(Coordinate(20,20,0)); // Example position for line/arrow
    markFields.setType(InfoMarkTypeEnum::TEXT);
    InfomarkId newMarkId = mapData.addMarker(markFields);
    QVERIFY(newMarkId != INVALID_INFOMARK_ID);

    // Verify current marks changed
    QVERIFY(!mapData.getCurrentMarks().empty());
    QVERIFY(mapData.getCurrentMarks().getRawCopy(newMarkId).getText() == InfoMarkText("Test Mark"));

    // Snapshot should still be the initial empty state
    QCOMPARE(mapData.getSnapshotMarks(), initialMarks);
    QVERIFY(mapData.getSnapshotMarks() != mapData.getCurrentMarks());

    // --- Revert to snapshot ---
    mapData.revertToSnapshot();

    // Verify current state is back to initial (empty) state
    QCOMPARE(mapData.getCurrentMap(), initialMap);
    QCOMPARE(mapData.getCurrentMarks(), initialMarks);
    QVERIFY(mapData.getCurrentMap().empty()); // Double check
    QVERIFY(mapData.getCurrentMarks().empty()); // Double check

    // --- Test with non-empty initial state for snapshot ---
    // mapData is now empty. Add something and snapshot it.
    mapData.applySingleChange(pc, addRoomChange); // roomCoord(1,2,0)
    newRoomId = mapData.getCurrentMap().findAllRooms(roomCoord).first();
    mapData.applySingleChange(pc, Change{room_change_types::SetRoomName{newRoomId, RoomName("Setup Room")}});
    newMarkId = mapData.addMarker(markFields); // "Test Mark"

    Map midStateMap = mapData.getCurrentMap();
    InfomarkDb midStateMarks = mapData.getCurrentMarks();
    QVERIFY(!midStateMap.empty());
    QVERIFY(!midStateMarks.empty());

    mapData.takeSnapshot(); // Snapshot of "Setup Room" and "Test Mark"
    QCOMPARE(mapData.getSnapshotMap(), midStateMap);
    QCOMPARE(mapData.getSnapshotMarks(), midStateMarks);

    // Make more changes
    Coordinate anotherRoomCoord(3,3,0);
    Change addAnotherRoomChange{room_change_types::AddPermanentRoom{anotherRoomCoord}};
    mapData.applySingleChange(pc, addAnotherRoomChange);
    RoomId anotherRoomId = mapData.getCurrentMap().findAllRooms(anotherRoomCoord).first();
    mapData.applySingleChange(pc, Change{room_change_types::SetRoomName{anotherRoomId, RoomName("Another Room")}});

    InfoMarkFields markFields2;
    markFields2.setText(InfoMarkText("Another Mark"));
    markFields2.setPosition1(Coordinate(30,30,0));
    markFields2.setType(InfoMarkTypeEnum::TEXT);
    mapData.addMarker(markFields2);

    QVERIFY(mapData.getCurrentMap() != midStateMap);
    QVERIFY(mapData.getCurrentMarks() != midStateMarks);

    mapData.revertToSnapshot(); // Revert to "Setup Room" and "Test Mark"
    QCOMPARE(mapData.getCurrentMap(), midStateMap);
    QCOMPARE(mapData.getCurrentMarks(), midStateMarks);
    QVERIFY(mapData.getCurrentMap().findAllRooms(anotherRoomCoord).empty()); // "Another room" should be gone
    // Check that "Another Mark" is gone (this is a bit trickier as IDs might recycle)
    // A simpler check: count marks or verify specific content of midStateMarks.
    QCOMPARE(mapData.getCurrentMarks().size(), midStateMarks.size());

}

QTEST_MAIN(TestMainWindow)
