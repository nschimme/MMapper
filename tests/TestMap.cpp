// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "TestMap.h"

#include "../src/global/HideQDebug.h"
#include "../src/map/Diff.h"
#include "../src/global/CopyOnWrite.h"
#include "../src/global/progresscounter.h"
#include "../src/map/Diff.h"
#include "../src/map/Map.h"
#include "../src/map/SpatialDb.h"
#include "../src/map/TinyRoomIdSet.h"
#include "../src/map/sanitizer.h"
#include "../src/map/World.h"      // Added for TestCow tests
#include "../src/map/RawRoom.h"     // Added for TestCow tests
#include "../src/map/RoomFields.h"  // Added for TestCow tests
#include "../src/map/Changes.h"     // Added for TestCow tests
#include "../src/map/mmapper2room.h"// Added for makeRoomName
#include <memory>                 // Added for std::make_shared

#include <QDebug>
#include <QtTest/QtTest>

// Helper from TestCow.cpp
namespace { // Encapsulate in anonymous namespace
RawRoom createTestRawRoom(RoomId id, const std::string &name_str = "TestRoom")
{
    RawRoom room;
    room.id = id;
    room.fields.Name = makeRoomName(name_str);
    room.status = RoomStatusEnum::Permanent;
    return room;
}
} // namespace

TestMap::TestMap() = default;

TestMap::~TestMap() = default;

void TestMap::diffTest()
{
    mmqt::HideQDebug forThisTest;
    test::testMapDiff();
}

void TestMap::mapTest()
{
    mmqt::HideQDebug forThisTest;
    test::testMap();
}

void TestMap::sanitizerTest()
{
    mmqt::HideQDebug forThisTest;
    test::testSanitizer();
}

void TestMap::tinyRoomIdSetTest()
{
    mmqt::HideQDebug forThisTest;
    test::testTinyRoomIdSet();
}

void TestMap::spatialDbCowTest()
{
    mmqt::HideQDebug forThisTest;
    ProgressCounter pc; // Dummy progress counter for updateBounds

    // --- Basic Functionality Tests (largely from previous version) ---
    {
        SpatialDb db;
        QVERIFY(db.getBounds() == std::nullopt);
        QCOMPARE(db.size(), 0u);

        db.add(RoomId{1}, Coordinate{0,0,0});
        QCOMPARE(db.size(), 1u);
        const RoomId* foundRoom = db.findUnique(Coordinate{0,0,0});
        QVERIFY(foundRoom != nullptr);
        QCOMPARE(*foundRoom, RoomId{1});

        db.add(RoomId{2}, Coordinate{1,1,1});
        QCOMPARE(db.size(), 2u);
        const RoomId* foundRoom2 = db.findUnique(Coordinate{1,1,1});
        QVERIFY(foundRoom2 != nullptr);
        QCOMPARE(*foundRoom2, RoomId{2});
    }

    // --- Refined COW Tests using Copy Semantics ---

    // 1. Test COW on `add`
    {
        SpatialDb db1;
        db1.add(RoomId{1}, Coordinate{0,0,0});

        SpatialDb db2 = db1; // Copy construction, m_unique should share data
        QVERIFY(db1 == db2);
        QCOMPARE(db1.size(), 1u);
        QCOMPARE(db2.size(), 1u);

        db2.add(RoomId{2}, Coordinate{1,1,1}); // Should trigger COW for db2.m_unique

        QVERIFY(db1 != db2);
        // Verify db1's state (should be unchanged)
        QCOMPARE(db1.size(), 1u);
        QVERIFY(db1.findUnique(Coordinate{0,0,0}) != nullptr);
        QCOMPARE(*(db1.findUnique(Coordinate{0,0,0})), RoomId{1});
        QVERIFY(db1.findUnique(Coordinate{1,1,1}) == nullptr);

        // Verify db2's state (should have the new room)
        QCOMPARE(db2.size(), 2u);
        QVERIFY(db2.findUnique(Coordinate{0,0,0}) != nullptr);
        QCOMPARE(*(db2.findUnique(Coordinate{0,0,0})), RoomId{1});
        QVERIFY(db2.findUnique(Coordinate{1,1,1}) != nullptr);
        QCOMPARE(*(db2.findUnique(Coordinate{1,1,1})), RoomId{2});
    }

    // 2. Test COW on `remove`
    {
        SpatialDb db1;
        db1.add(RoomId{10}, Coordinate{10,10,10});
        db1.add(RoomId{11}, Coordinate{11,11,11});

        SpatialDb db2 = db1; // Copy, m_unique shares data
        QVERIFY(db1 == db2);
        QCOMPARE(db1.size(), 2u);

        db2.remove(RoomId{10}, Coordinate{10,10,10}); // Triggers COW for db2.m_unique

        QVERIFY(db1 != db2);
        // Verify db1's state (should be unchanged)
        QCOMPARE(db1.size(), 2u);
        QVERIFY(db1.findUnique(Coordinate{10,10,10}) != nullptr);
        QVERIFY(db1.findUnique(Coordinate{11,11,11}) != nullptr);

        // Verify db2's state (room removed)
        QCOMPARE(db2.size(), 1u);
        QVERIFY(db2.findUnique(Coordinate{10,10,10}) == nullptr);
        QVERIFY(db2.findUnique(Coordinate{11,11,11}) != nullptr);
        QCOMPARE(*(db2.findUnique(Coordinate{11,11,11})), RoomId{11});
    }

    // 3. Test COW on `move`
    {
        SpatialDb db1;
        db1.add(RoomId{20}, Coordinate{20,20,20});

        SpatialDb db2 = db1; // Copy, m_unique shares data
        QVERIFY(db1 == db2);
        QCOMPARE(db1.size(), 1u);

        db2.move(RoomId{20}, Coordinate{20,20,20}, Coordinate{21,21,21}); // Triggers COW

        QVERIFY(db1 != db2);
        // Verify db1's state (original position)
        QCOMPARE(db1.size(), 1u);
        QVERIFY(db1.findUnique(Coordinate{20,20,20}) != nullptr);
        QCOMPARE(*(db1.findUnique(Coordinate{20,20,20})), RoomId{20});
        QVERIFY(db1.findUnique(Coordinate{21,21,21}) == nullptr);

        // Verify db2's state (new position)
        QCOMPARE(db2.size(), 1u);
        QVERIFY(db2.findUnique(Coordinate{20,20,20}) == nullptr);
        QVERIFY(db2.findUnique(Coordinate{21,21,21}) != nullptr);
        QCOMPARE(*(db2.findUnique(Coordinate{21,21,21})), RoomId{20});
    }

    // 4. Test `updateBounds` interaction with COW
    {
        SpatialDb db1;
        db1.add(RoomId{30}, Coordinate{30,30,30});
        db1.updateBounds(pc);
        auto initial_bounds_db1 = db1.getBounds();
        QVERIFY(initial_bounds_db1.has_value());
        QCOMPARE(initial_bounds_db1->min, (Coordinate{30,30,30}));
        QCOMPARE(initial_bounds_db1->max, (Coordinate{30,30,30}));

        SpatialDb db2 = db1; // Copy, m_unique shares, m_bounds copied, m_needsBoundsUpdate copied
        QVERIFY(db1 == db2);
        QCOMPARE(db1.getBounds(), db2.getBounds());
        QVERIFY(!db2.needsBoundsUpdate());


        db2.add(RoomId{31}, Coordinate{0,0,0}); // Triggers COW for db2.m_unique, sets db2.m_needsBoundsUpdate = true (implicitly by add)
        QVERIFY(db2.needsBoundsUpdate()); // add should have either updated bounds or marked for update
                                          // The current SpatialDb::add *always* updates bounds eagerly or marks.
                                          // Let's ensure it's marked if not updated, or check updated bounds.
                                          // SpatialDb::add updates m_bounds directly. So m_needsBoundsUpdate might be false.
                                          // Let's check the bounds of db2 before explicit updateBounds call.

        auto bounds_db2_after_add = db2.getBounds();
        QVERIFY(bounds_db2_after_add.has_value());
        // Depending on SpatialDb::add's behavior, bounds might be immediately updated or require updateBounds().
        // Current SpatialDb::add updates bounds directly.
        QCOMPARE(bounds_db2_after_add->min, (Coordinate{0,0,0}));
        QCOMPARE(bounds_db2_after_add->max, (Coordinate{30,30,30}));


        db2.updateBounds(pc); // Explicitly call updateBounds
        auto final_bounds_db2 = db2.getBounds();
        QVERIFY(final_bounds_db2.has_value());
        QCOMPARE(final_bounds_db2->min, (Coordinate{0,0,0}));
        QCOMPARE(final_bounds_db2->max, (Coordinate{30,30,30}));

        QVERIFY(db1 != db2); // Because m_unique differs

        // Verify db1.getBounds() is based on RoomId{30} only and is unchanged
        auto final_bounds_db1 = db1.getBounds();
        QVERIFY(final_bounds_db1.has_value());
        QCOMPARE(final_bounds_db1->min, (Coordinate{30,30,30}));
        QCOMPARE(final_bounds_db1->max, (Coordinate{30,30,30}));
        QCOMPARE(db1.getBounds(), initial_bounds_db1); // Ensure db1's bounds didn't change
    }

    // Test `operator==` and `operator!=` (already well covered by COW tests above but good to have explicit simple cases)
    {
        SpatialDb db_eq1, db_eq2;
        db_eq1.add(RoomId{1}, Coordinate{0,0,0});
        db_eq1.add(RoomId{2}, Coordinate{1,1,1});

        db_eq2.add(RoomId{1}, Coordinate{0,0,0});
        db_eq2.add(RoomId{2}, Coordinate{1,1,1});

        QVERIFY(db_eq1 == db_eq2);
        QVERIFY(!(db_eq1 != db_eq2));

        SpatialDb db_eq3 = db_eq1; // Copy
        QVERIFY(db_eq1 == db_eq3);

        db_eq3.add(RoomId{3}, Coordinate{2,2,2}); // Modifies db_eq3, triggering COW
        QVERIFY(db_eq1 != db_eq3);
        QVERIFY(!(db_eq1 == db_eq3));
        QCOMPARE(db_eq1.size(), 2u); // db_eq1 should be unchanged
        QCOMPARE(db_eq3.size(), 3u);
    }
}

// --- Methods from TestCow (generic tests removed, World-specific tests remain) ---

void TestMap::testWorldCopyCOW()
{
    World w1;
    ProgressCounter pc;
    // Add a room to w1 - using apply directly on world for simplicity in test setup
    RoomId test_room_id{0}; // Placeholder, will be assigned by AddPermanentRoom
    // RawRoom r1_data = createTestRawRoom(test_room_id, "RoomInW1"); // ID in RawRoom not used by AddPermanentRoom change

    World world1_for_copy;
    world1_for_copy.applyOne(pc, Change{room_change_types::AddPermanentRoom{Coordinate{0, 0, 0}}});
    QVERIFY(!world1_for_copy.getRoomSet().empty());
    RoomId room_id_in_w1 = world1_for_copy.getRoomSet().first();
    world1_for_copy.applyOne(pc,
                             Change{room_change_types::ModifyRoomFlags{room_id_in_w1,
                                                                       makeRoomName("OriginalName"),
                                                                       FlagModifyModeEnum::ASSIGN}});

    std::shared_ptr<const RawRoom> r1_w1_ptr = world1_for_copy.getRoom(room_id_in_w1);
    QVERIFY(r1_w1_ptr != nullptr);

    World world2 = world1_for_copy.copy();
    std::shared_ptr<const RawRoom> r1_w2_ptr = world2.getRoom(room_id_in_w1);
    QVERIFY(r1_w2_ptr != nullptr);

    // Data should be shared initially
    QCOMPARE(static_cast<const void *>(r1_w1_ptr.get()), static_cast<const void *>(r1_w2_ptr.get()));
    QCOMPARE(r1_w1_ptr->fields.Name.toStdStringUtf8(), "OriginalName");

    // Modify room in world2
    world2.applyOne(pc,
                    Change{room_change_types::ModifyRoomFlags{room_id_in_w1,
                                                              makeRoomName("ModifiedInW2"),
                                                              FlagModifyModeEnum::ASSIGN}});

    std::shared_ptr<const RawRoom> r1_w2_modified_ptr = world2.getRoom(room_id_in_w1);
    QVERIFY(r1_w2_modified_ptr != nullptr);

    // Data should now be different for W2's room
    QVERIFY(static_cast<const void *>(r1_w1_ptr.get())
            != static_cast<const void *>(r1_w2_modified_ptr.get()));
    QCOMPARE(r1_w1_ptr->fields.Name.toStdStringUtf8(), "OriginalName");          // W1 unchanged
    QCOMPARE(r1_w2_modified_ptr->fields.Name.toStdStringUtf8(), "ModifiedInW2"); // W2 changed
}

void TestMap::testWorldApplyChangeCOW()
{
    World world;
    ProgressCounter pc;
    world.applyOne(pc, Change{room_change_types::AddPermanentRoom{Coordinate{0, 0, 0}}});
    QVERIFY(!world.getRoomSet().empty());
    RoomId room_id = world.getRoomSet().first();
    world.applyOne(pc,
                   Change{room_change_types::ModifyRoomFlags{room_id,
                                                             makeRoomName("Initial"),
                                                             FlagModifyModeEnum::ASSIGN}});

    std::shared_ptr<const RawRoom> room_ptr_before_change = world.getRoom(room_id);
    const void *addr_before = room_ptr_before_change.get();

    // Apply a change that modifies the room
    world.applyOne(pc,
                   Change{room_change_types::ModifyRoomFlags{room_id,
                                                             makeRoomName("ChangedName"),
                                                             FlagModifyModeEnum::ASSIGN}});

    std::shared_ptr<const RawRoom> room_ptr_after_change = world.getRoom(room_id);
    const void *addr_after = room_ptr_after_change.get();

    QVERIFY(room_ptr_after_change != nullptr);
    QCOMPARE(room_ptr_after_change->fields.Name.toStdStringUtf8(), "ChangedName");

    // Check if a copy was made (address should be different)
    QVERIFY(addr_before != addr_after);
}


QTEST_MAIN(TestMap)
