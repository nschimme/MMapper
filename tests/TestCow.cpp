// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include "TestCow.h"

#include "global/progresscounter.h" // For ProgressCounter
#include "map/Changes.h"            // For change types
#include "map/CowRoom.h"
#include "map/Map.h" // For applying changes to world via Map
#include "map/RawRoom.h"
#include "map/RoomFields.h" // For RoomName, etc.
#include "map/World.h"
#include "map/mmapper2room.h" // For makeRoomName

#include <memory> // For std::make_shared

#include <QtTest/QtTest>

// Helper to create a basic RawRoom for testing
RawRoom createTestRawRoom(RoomId id, const std::string &name_str = "TestRoom")
{
    RawRoom room;
    room.id = id;
    room.fields.Name = makeRoomName(name_str);
    room.status = RoomStatusEnum::Permanent;
    return room;
}

void TestCow::testReadOnlySharing()
{
    auto raw_room_data = std::make_shared<RawRoom>(createTestRawRoom(RoomId{1}));
    CowRoom c1(raw_room_data);
    CowRoom c2 = c1; // Copy constructor of CowRoom (default, copies variant which copies shared_ptr)

    QCOMPARE(static_cast<const void *>(c1.get().get()), static_cast<const void *>(c2.get().get()));
    QVERIFY(c1.get().use_count()
            > 1); // Both c1 and c2 should share it, plus raw_room_data if still in scope
    QCOMPARE(c1.get()->fields.Name.toStdStringUtf8(), "TestRoom");
}

void TestCow::testLazyCopyOnWrite()
{
    auto initial_raw_room = std::make_shared<const RawRoom>(createTestRawRoom(RoomId{1}, "Initial"));
    CowRoom c1(initial_raw_room);

    const RawRoom *initial_addr = initial_raw_room.get();

    // First get should be the shared one
    QCOMPARE(static_cast<const void *>(c1.get().get()), static_cast<const void *>(initial_addr));

    // This should trigger a copy
    std::shared_ptr<RawRoom> writable_room_ptr = c1.getMutable();
    QVERIFY(static_cast<const void *>(writable_room_ptr.get())
            != static_cast<const void *>(initial_addr));
    QCOMPARE(writable_room_ptr->fields.Name.toStdStringUtf8(),
             "Initial"); // Content should be copied

    // Modify the copy
    writable_room_ptr->fields.Name = makeRoomName("Modified");
    QCOMPARE(c1.get()->fields.Name.toStdStringUtf8(),
             "Modified"); // c1.get() should now point to the modified copy
    QCOMPARE(initial_raw_room->fields.Name.toStdStringUtf8(),
             "Initial"); // Original const shared_ptr data unchanged
}

void TestCow::testMutationIsolation()
{
    auto r1_const_ptr = std::make_shared<const RawRoom>(createTestRawRoom(RoomId{1}, "OriginalR1"));
    CowRoom c1(r1_const_ptr);
    CowRoom c2 = c1; // c1 and c2 share data

    QVERIFY(static_cast<const void *>(c1.get().get()) == static_cast<const void *>(c2.get().get()));

    // Mutate c1
    std::shared_ptr<RawRoom> c1_writable = c1.getMutable();
    c1_writable->fields.Name = makeRoomName("C1 Modified");

    QVERIFY(static_cast<const void *>(c1.get().get())
            != static_cast<const void *>(c2.get().get())); // c1 should have a new copy
    QCOMPARE(c1.get()->fields.Name.toStdStringUtf8(), "C1 Modified");
    QCOMPARE(c2.get()->fields.Name.toStdStringUtf8(), "OriginalR1"); // c2 should be unaffected
}

void TestCow::testFinalize()
{
    CowRoom c1(std::make_shared<RawRoom>(createTestRawRoom(RoomId{1})));

    std::shared_ptr<RawRoom> r_writable = c1.getMutable();
    const void *writable_addr1 = r_writable.get();
    r_writable->fields.Name = makeRoomName("TempWrite");

    c1.finalize();
    QVERIFY(!c1.isWritable()); // Assuming isWritable() checks if variant holds shared_ptr<RawRoom>

    // Getting const should still work and point to the data that was finalized
    QCOMPARE(c1.get()->fields.Name.toStdStringUtf8(), "TempWrite");
    const void *const_addr_after_finalize = c1.get().get();
    QCOMPARE(const_addr_after_finalize,
             writable_addr1); // Should be same address, just const_cast by finalize

    // Getting mutable again should create a new copy
    std::shared_ptr<RawRoom> r_writable2 = c1.getMutable();
    QVERIFY(static_cast<const void *>(r_writable2.get()) != writable_addr1);
    QCOMPARE(r_writable2->fields.Name.toStdStringUtf8(), "TempWrite"); // Content is copied
}

void TestCow::testWorldCopyCOW()
{
    World w1;
    ProgressCounter pc;
    // Add a room to w1 - using apply directly on world for simplicity in test setup
    RoomId test_room_id{0};
    RawRoom r1_data = createTestRawRoom(test_room_id, "RoomInW1");
    // World::setRoom_lowlevel or similar direct modification path for setup
    // For testing, we need to ensure the room is added.
    // Let's use the apply mechanism as that's the public API for changes.
    // To do this, we need a Map object.
    //Map m1(std::move(w1)); // Takes ownership of w1
    //MapApplyResult result = m1.applySingleChange(pc, Change{room_change_types::AddPermanentRoom{r1_data.position}});
    //m1 = result.map;
    // Find the added room's ID. AddPermanentRoom might assign a new ID.
    // For simplicity, assume addRoom works and we can find it.
    // Let's manually insert for more direct control in this test if apply is too complex to setup:

    World world1_for_copy;
    world1_for_copy.applyOne(pc, Change{room_change_types::AddPermanentRoom{Coordinate{0, 0, 0}}});
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

void TestCow::testWorldApplyChangeCOW()
{
    World world;
    ProgressCounter pc;
    world.applyOne(pc, Change{room_change_types::AddPermanentRoom{Coordinate{0, 0, 0}}});
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
    // This assumes that the getRoom(id) after modification doesn't return the exact same shared_ptr
    // if the underlying object was copy-on-written. The CowRoom's internal shared_ptr would change.
    QVERIFY(addr_before != addr_after);
}

QTEST_MAIN(TestCow)
