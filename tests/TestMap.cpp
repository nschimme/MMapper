// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "TestMap.h"

#include "../src/global/HideQDebug.h"
#include "../src/global/progresscounter.h"
#include "../src/map/Diff.h"
#include "../src/map/Map.h"
#include "../src/map/TinyRoomIdSet.h"
#include "../src/map/sanitizer.h"
#include "../src/mapdata/SubAreaDetector.h"

#include <QDebug>
#include <QtTest/QtTest>

TestMap::TestMap() = default;

TestMap::~TestMap() = default;

void TestMap::diffTest()
{
    Map::enableExtraSanityChecks(true);
    mmqt::HideQDebug forThisTest;
    test::testMapDiff();
}

void TestMap::mapTest()
{
    Map::enableExtraSanityChecks(true);
    mmqt::HideQDebug forThisTest;
    test::testMap();
}

void TestMap::sanitizerTest()
{
    Map::enableExtraSanityChecks(true);
    mmqt::HideQDebug forThisTest;
    test::testSanitizer();
}

void TestMap::tinyRoomIdSetTest()
{
    Map::enableExtraSanityChecks(true);
    mmqt::HideQDebug forThisTest;
    test::testTinyRoomIdSet();
}

void TestMap::roomIdSetTest()
{
    Map::enableExtraSanityChecks(true);
    mmqt::HideQDebug forThisTest;
    test::testRoomIdSet();
    test::testImmRoomIdSet();
}

void TestMap::subAreaDetectorTest()
{
    Map::enableExtraSanityChecks(true);
    mmqt::HideQDebug forThisTest;

    // Test Empty Map
    {
        ProgressCounter pc;
        MapPair pair = Map::fromRooms(pc, {}, {});
        const SubAreaInfo info = SubAreaDetector::detectSubAreas(pair.modified);

        QVERIFY(info.roomClusterId.empty());
        QVERIFY(info.selfLoops.empty());
        QVERIFY(info.internalConnections.empty());
        QVERIFY(info.boundaryExitConnections.empty());
    }

    // Test Linear Tunnel / Pathway Detection
    {
        ProgressCounter pc;
        ExternalRawRoom r1;
        r1.id = ExternalRoomId{1};
        r1.position = Coordinate{0, 0, 0};
        r1.setName(makeRoomName("Linear 1"));
        r1.exits[ExitDirEnum::EAST].setExitFlags(ExitFlags{ExitFlagEnum::EXIT});
        r1.exits[ExitDirEnum::EAST].outgoing.insert(ExternalRoomId{2});
        r1.exits[ExitDirEnum::EAST].incoming.insert(ExternalRoomId{2});

        ExternalRawRoom r2;
        r2.id = ExternalRoomId{2};
        r2.position = Coordinate{1, 0, 0};
        r2.setName(makeRoomName("Linear 2"));
        r2.exits[ExitDirEnum::WEST].setExitFlags(ExitFlags{ExitFlagEnum::EXIT});
        r2.exits[ExitDirEnum::WEST].outgoing.insert(ExternalRoomId{1});
        r2.exits[ExitDirEnum::WEST].incoming.insert(ExternalRoomId{1});

        MapPair pair = Map::fromRooms(pc, {r1, r2}, {});
        const SubAreaInfo info = SubAreaDetector::detectSubAreas(pair.modified);

        const RoomId id1 = pair.modified.findRoomHandle(ExternalRoomId{1}).getId();
        const RoomId id2 = pair.modified.findRoomHandle(ExternalRoomId{2}).getId();

        QVERIFY(!info.isMazeRoom(id1));
        QVERIFY(!info.isMazeRoom(id2));
        QVERIFY(info.getFlags(id1).contains(SubAreaFlagEnum::TUNNEL));
        QVERIFY(info.getFlags(id2).contains(SubAreaFlagEnum::TUNNEL));
    }

    // Test Maze Cluster and Exit Connections
    {
        ProgressCounter pc;

        ExternalRawRoom r1;
        r1.id = ExternalRoomId{1};
        r1.position = Coordinate{0, 0, 0};
        r1.setDescription(makeRoomDesc("Deep in the forest"));
        r1.exits[ExitDirEnum::NORTH].setExitFlags(ExitFlags{ExitFlagEnum::EXIT});
        r1.exits[ExitDirEnum::NORTH].outgoing.insert(ExternalRoomId{2});
        r1.exits[ExitDirEnum::SOUTH].setExitFlags(ExitFlags{ExitFlagEnum::EXIT});
        r1.exits[ExitDirEnum::SOUTH].incoming.insert(ExternalRoomId{3});

        ExternalRawRoom r2;
        r2.id = ExternalRoomId{2};
        r2.position = Coordinate{0, 1, 0};
        r2.setDescription(makeRoomDesc("Deep in the forest"));
        r2.exits[ExitDirEnum::EAST].setExitFlags(ExitFlags{ExitFlagEnum::EXIT});
        r2.exits[ExitDirEnum::EAST].outgoing.insert(ExternalRoomId{3});
        r2.exits[ExitDirEnum::SOUTH].setExitFlags(ExitFlags{ExitFlagEnum::EXIT});
        r2.exits[ExitDirEnum::SOUTH].incoming.insert(ExternalRoomId{1});

        ExternalRawRoom r3;
        r3.id = ExternalRoomId{3};
        r3.position = Coordinate{1, 1, 0};
        r3.setDescription(makeRoomDesc("Deep in the forest"));
        r3.exits[ExitDirEnum::SOUTH].setExitFlags(ExitFlags{ExitFlagEnum::EXIT});
        r3.exits[ExitDirEnum::SOUTH].outgoing.insert(ExternalRoomId{1});
        r3.exits[ExitDirEnum::EAST].setExitFlags(ExitFlags{ExitFlagEnum::EXIT});
        r3.exits[ExitDirEnum::EAST].outgoing.insert(ExternalRoomId{100});
        r3.exits[ExitDirEnum::WEST].setExitFlags(ExitFlags{ExitFlagEnum::EXIT});
        r3.exits[ExitDirEnum::WEST].incoming.insert(ExternalRoomId{2});

        ExternalRawRoom r100;
        r100.id = ExternalRoomId{100};
        r100.position = Coordinate{2, 1, 0};
        r100.setName(makeRoomName("Clear Clearing"));
        r100.setDescription(makeRoomDesc("A sunny clearing"));
        r100.exits[ExitDirEnum::WEST].setExitFlags(ExitFlags{ExitFlagEnum::EXIT});
        r100.exits[ExitDirEnum::WEST].incoming.insert(ExternalRoomId{3});

        MapPair pair = Map::fromRooms(pc, {r1, r2, r3, r100}, {});
        const SubAreaInfo info = SubAreaDetector::detectSubAreas(pair.modified);

        const RoomId id1 = pair.modified.findRoomHandle(ExternalRoomId{1}).getId();
        const RoomId id2 = pair.modified.findRoomHandle(ExternalRoomId{2}).getId();
        const RoomId id3 = pair.modified.findRoomHandle(ExternalRoomId{3}).getId();
        const RoomId id100 = pair.modified.findRoomHandle(ExternalRoomId{100}).getId();

        QVERIFY(info.isMazeRoom(id1));
        QVERIFY(info.isMazeRoom(id2));
        QVERIFY(info.isMazeRoom(id3));
        QVERIFY(!info.isMazeRoom(id100));

        QVERIFY(info.isInternalConnection(id1, ExitDirEnum::NORTH));
        QVERIFY(info.isInternalConnection(id2, ExitDirEnum::EAST));
        QVERIFY(info.isInternalConnection(id3, ExitDirEnum::SOUTH));

        QVERIFY(info.isBoundaryExitConnection(id3, ExitDirEnum::EAST));
        QVERIFY(!info.isInternalConnection(id3, ExitDirEnum::EAST));
    }
}

QTEST_MAIN(TestMap)
