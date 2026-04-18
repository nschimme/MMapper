// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "TestShortestPath.h"

#include "../src/global/progresscounter.h"
#include "../src/map/Map.h"
#include "../src/map/RawRoom.h"
#include "../src/map/mmapper2room.h"
#include "../src/mapdata/mapdata.h"
#include "../src/mapdata/roomfilter.h"
#include "../src/mapdata/shortestpath.h"
#include "../src/parser/AbstractParser-Commands.h"

#include <QtTest/QtTest>

// Link-time stubs for getParserCommandName
Abbrev getParserCommandName(RoomTerrainEnum)
{
    return Abbrev("terrain", 1);
}
Abbrev getParserCommandName(RoomLightEnum)
{
    return Abbrev("light", 1);
}
Abbrev getParserCommandName(RoomRidableEnum)
{
    return Abbrev("ridable", 1);
}
Abbrev getParserCommandName(RoomAlignEnum)
{
    return Abbrev("align", 1);
}
Abbrev getParserCommandName(RoomStatusEnum)
{
    return Abbrev("status", 1);
}
Abbrev getParserCommandName(RoomPortableEnum)
{
    return Abbrev("portable", 1);
}
Abbrev getParserCommandName(RoomSundeathEnum)
{
    return Abbrev("sundeath", 1);
}
Abbrev getParserCommandName(RoomLoadFlagEnum)
{
    return Abbrev("load", 1);
}
Abbrev getParserCommandName(RoomMobFlagEnum)
{
    return Abbrev("mob", 1);
}
Abbrev getParserCommandName(ExitFlagEnum)
{
    return Abbrev("exit", 1);
}
Abbrev getParserCommandName(DoorFlagEnum)
{
    return Abbrev("door", 1);
}

namespace {

class TestRecipient final : public ShortestPathRecipient
{
public:
    ~TestRecipient() final;

public:
    std::vector<ShortestPathResult> results;

private:
    void virt_receiveShortestPath(const Map &, ShortestPathResult result) final
    {
        results.push_back(std::move(result));
    }
};

TestRecipient::~TestRecipient() = default;

} // namespace

TestShortestPath::TestShortestPath() = default;

TestShortestPath::~TestShortestPath() = default;

void TestShortestPath::shortestPathSearchTest()
{
    std::vector<ExternalRawRoom> rooms;
    ExternalRoomId er1{1}, er2{2}, er3{3};

    {
        ExternalRawRoom r1;
        r1.id = er1;
        r1.setName(makeRoomName("Room 1"));
        r1.getExit(ExitDirEnum::NORTH).getOutgoingSet().insert(er2);
        rooms.push_back(r1);

        ExternalRawRoom r2;
        r2.id = er2;
        r2.setName(makeRoomName("Room 2"));
        r2.getExit(ExitDirEnum::EAST).getOutgoingSet().insert(er3);
        rooms.push_back(r2);

        ExternalRawRoom r3;
        r3.id = er3;
        r3.setName(makeRoomName("Room 3"));
        rooms.push_back(r3);
    }

    ProgressCounter pc;
    auto mapPair = Map::fromRooms(pc, rooms, {});
    const Map &map = mapPair.modified;

    auto r1_handle = map.findRoomHandle(er1);
    QVERIFY(r1_handle.exists());
    RoomId ir1 = r1_handle.getId();

    auto optFilter = RoomFilter::parseRoomFilter("Room 3");
    QVERIFY(optFilter.has_value());

    {
        TestRecipient recipient;
        // North + East = 0.75 (Indoors R2) + 0.75 (Indoors R3) = 1.5
        MapData::shortestPathSearch(r1_handle, recipient, *optFilter, 1, 0);

        QCOMPARE(recipient.results.size(), 1ULL);
        QCOMPARE(recipient.results[0].path.size(), 2ULL);
        QCOMPARE(recipient.results[0].path[0], ExitDirEnum::NORTH);
        QCOMPARE(recipient.results[0].path[1], ExitDirEnum::EAST);
    }

    // Test including start room
    auto optStartFilter = RoomFilter::parseRoomFilter("Room 1");
    QVERIFY(optStartFilter.has_value());
    {
        TestRecipient startRecipient;
        MapData::shortestPathSearch(r1_handle, startRecipient, *optStartFilter, 10, 0);
        QCOMPARE(startRecipient.results.size(), 1ULL);
        QCOMPARE(startRecipient.results[0].id.asUint32(), ir1.asUint32());
        QCOMPARE(startRecipient.results[0].path.size(), 0ULL);
    }

    // Test max_dist cutoff
    // R1->R2 cost is 0.75 (Indoors)
    // R2->R3 cost is 0.75 (Indoors)
    // Total R1->R3 is 1.5
    {
        TestRecipient cutoffRecipient;
        // Limit to 1.0, should reach R2 but not R3.
        auto optAllFilter = RoomFilter::parseRoomFilter("Room");
        QVERIFY(optAllFilter.has_value());
        MapData::shortestPathSearch(r1_handle, cutoffRecipient, *optAllFilter, 10, 1.0);

        // Should have R1 (dist 0) and R2 (dist 0.75)
        QCOMPARE(cutoffRecipient.results.size(), 2ULL);
        QCOMPARE(cutoffRecipient.results[0].id.asUint32(), ir1.asUint32());
        QCOMPARE(cutoffRecipient.results[1].id.asUint32(),
                 map.findRoomHandle(er2).getId().asUint32());
    }
}

QTEST_MAIN(TestShortestPath)
