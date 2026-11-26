// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/progresscounter.h"
#include "../global/tests.h"
#include "../opengl/RoomQuadTag.h"
#include "Change.h"
#include "ChangeTypes.h"
#include "ExplorerSecrets.h"

namespace { // anonymous

struct NODISCARD Fixture final
{
    ProgressCounter pc;
    Map map;

    void apply(Change change) { map = map.applySingleChange(pc, change).map; }

    NODISCARD RoomId add(const int x, const uint32_t serverId)
    {
        const Coordinate pos{x, 0, 0};
        apply(Change{room_change_types::AddPermanentRoom{pos}});
        const RoomId id = map.findRoomHandle(pos).getId();
        apply(Change{room_change_types::SetServerId{id, ServerRoomId{serverId}}});
        return id;
    }

    void connect(const RoomId from, const ExitDirEnum dir, const RoomId to)
    {
        apply(Change{exit_change_types::ModifyExitConnection{ChangeTypeEnum::Add,
                                                             from,
                                                             dir,
                                                             to,
                                                             WaysEnum::TwoWay}});
    }

    void hide(const RoomId room, const ExitDirEnum dir)
    {
        apply(Change{exit_change_types::SetExitFlags{FlagChangeEnum::Set,
                                                     room,
                                                     dir,
                                                     ExitFlags{ExitFlagEnum::DOOR}}});
        apply(Change{exit_change_types::SetDoorFlags{FlagChangeEnum::Set,
                                                     room,
                                                     dir,
                                                     DoorFlags{DoorFlagEnum::HIDDEN}}});
    }
};

NODISCARD DiscoveredExit found(const uint32_t room, const ExitDirEnum dir)
{
    return DiscoveredExit{ServerRoomId{room}, dir};
}

// Applies the single-find patches to an empty mask of the same shape.
NODISCARD ExplorerFoundMask patched(const Map &map, const DiscoveredExits &discovered)
{
    ExplorerFoundMask mask = buildExplorerFoundMask(map, {});
    for (const DiscoveredExit &e : discovered) {
        for (const FoundSecretTexel &t : foundSecretTexels(map, e)) {
            TEST_ASSERT(mask.contains(t.cell));
            mask.texels[mask.indexOf(t.cell)] |= foundSecretBit(t.dir);
        }
    }
    return mask;
}

void testBits()
{
    uint8_t all = 0;
    for (const ExitDirEnum dir : ALL_EXITS_NESWUD) {
        const uint8_t bit = foundSecretBit(dir);
        TEST_ASSERT(bit != 0 && (bit & (bit - 1)) == 0); // exactly one bit
        TEST_ASSERT((all & bit) == 0);                   // distinct
        all = static_cast<uint8_t>(all | bit);
    }
    TEST_ASSERT(all == 0x3F);
}

void testFoundMask()
{
    Fixture f;
    const RoomId a = f.add(0, 10);
    const RoomId b = f.add(1, 11);
    const RoomId c = f.add(2, 12);
    const RoomId d = f.add(3, 13);
    const RoomId e = f.add(4, 14);
    // a <-> b: a secret door seen from both sides
    f.connect(a, ExitDirEnum::EAST, b);
    f.hide(a, ExitDirEnum::EAST);
    f.hide(b, ExitDirEnum::WEST);
    // b <-> c: an ordinary exit
    f.connect(b, ExitDirEnum::EAST, c);
    // c <-> d: a secret only on c's side
    f.connect(c, ExitDirEnum::EAST, d);
    f.hide(c, ExitDirEnum::EAST);
    (void) e;

    const Coordinate ca{0, 0, 0};
    const Coordinate cb{1, 0, 0};
    const Coordinate cc{2, 0, 0};
    const Coordinate cd{3, 0, 0};

    const auto none = buildExplorerFoundMask(f.map, {});
    TEST_ASSERT(!none.empty());
    TEST_ASSERT(none.width == 5 && none.height == 1 && none.depth == 1);
    for (const uint8_t t : none.texels) {
        TEST_ASSERT(t == 0);
    }

    // Found from a's side: a's cell, and the far side of the same door.
    DiscoveredExits own{found(10, ExitDirEnum::EAST)};
    auto m = buildExplorerFoundMask(f.map, own);
    TEST_ASSERT(m.isFound(ca, ExitDirEnum::EAST));
    TEST_ASSERT(m.isFound(cb, ExitDirEnum::WEST));
    TEST_ASSERT(!m.isFound(ca, ExitDirEnum::WEST));
    TEST_ASSERT(m.at(cc) == 0 && m.at(cd) == 0);
    // An ordinary exit never gets a bit, whatever is found.
    TEST_ASSERT(!m.isFound(cb, ExitDirEnum::EAST));

    // Found only from the far side: the same two texels.
    DiscoveredExits far{found(11, ExitDirEnum::WEST)};
    const auto mFar = buildExplorerFoundMask(f.map, far);
    TEST_ASSERT(mFar.texels == m.texels);

    // A one-sided secret: only the room that has it.
    m = buildExplorerFoundMask(f.map, DiscoveredExits{found(12, ExitDirEnum::EAST)});
    TEST_ASSERT(m.isFound(cc, ExitDirEnum::EAST));
    TEST_ASSERT(m.at(cd) == 0);

    // Finds that name an ordinary exit, another direction or an unknown room open nothing.
    m = buildExplorerFoundMask(f.map,
                               DiscoveredExits{found(11, ExitDirEnum::EAST),
                                               found(10, ExitDirEnum::NORTH),
                                               found(99, ExitDirEnum::EAST)});
    for (const uint8_t t : m.texels) {
        TEST_ASSERT(t == 0);
    }

    // Outside the bounds reads as not found.
    TEST_ASSERT(!m.isFound(Coordinate{100, 0, 0}, ExitDirEnum::EAST));
    TEST_ASSERT(!ExplorerFoundMask{}.isFound(ca, ExitDirEnum::EAST));
}

void testSingleFindPatches()
{
    Fixture f;
    const RoomId a = f.add(0, 10);
    const RoomId b = f.add(1, 11);
    const RoomId c = f.add(2, 12);
    const RoomId d = f.add(3, 13);
    f.connect(a, ExitDirEnum::EAST, b);
    f.hide(a, ExitDirEnum::EAST);
    f.hide(b, ExitDirEnum::WEST);
    f.connect(b, ExitDirEnum::EAST, c);
    f.connect(c, ExitDirEnum::EAST, d);
    f.hide(c, ExitDirEnum::EAST);

    // Both sides of a door from one find, and nothing for what is not a secret.
    TEST_ASSERT(foundSecretTexels(f.map, found(10, ExitDirEnum::EAST)).size() == 2);
    TEST_ASSERT(foundSecretTexels(f.map, found(11, ExitDirEnum::WEST)).size() == 2);
    TEST_ASSERT(foundSecretTexels(f.map, found(12, ExitDirEnum::EAST)).size() == 1);
    TEST_ASSERT(foundSecretTexels(f.map, found(11, ExitDirEnum::EAST)).empty());
    TEST_ASSERT(foundSecretTexels(f.map, found(99, ExitDirEnum::EAST)).empty());
    TEST_ASSERT(foundSecretTexels(f.map, found(10, ExitDirEnum::UNKNOWN)).empty());
    TEST_ASSERT(foundSecretTexels(f.map, found(10, ExitDirEnum::NONE)).empty());

    // Patching finds one by one gives the mask the worker builds from all of them.
    for (const DiscoveredExits &set :
         {DiscoveredExits{},
          DiscoveredExits{found(10, ExitDirEnum::EAST)},
          DiscoveredExits{found(11, ExitDirEnum::WEST)},
          DiscoveredExits{found(12, ExitDirEnum::EAST)},
          DiscoveredExits{found(10, ExitDirEnum::EAST), found(12, ExitDirEnum::EAST)}}) {
        TEST_ASSERT(patched(f.map, set).texels == buildExplorerFoundMask(f.map, set).texels);
    }

    // No rooms, no bounds, no mask.
    TEST_ASSERT(buildExplorerFoundMask(Map{}, {}).empty());
}

void testQuadTag()
{
    using room_quad_tag::FormEnum;
    // Untagged is exactly the old word: no tag bits at all.
    TEST_ASSERT(room_quad_tag::pack(FormEnum::NONE, 0) == 0);
    TEST_ASSERT(room_quad_tag::pack(FormEnum::NONE, 5) == 0);

    for (const FormEnum form : {FormEnum::NONE, FormEnum::DOOR, FormEnum::WALL}) {
        for (unsigned dir = 0; dir <= 5; ++dir) {
            const int tag = room_quad_tag::pack(form, dir);
            TEST_ASSERT((tag & ~room_quad_tag::TAG_BITS) == 0);
            TEST_ASSERT(tag >= 0);
            for (const int texZ : {0, 1, 17, 255}) {
                for (const int color : {0, 1, 31, 255}) {
                    const int word = (color << 8) | texZ | tag;
                    // the tag does not disturb the fields that were already there...
                    TEST_ASSERT(room_quad_tag::texZOf(word) == texZ);
                    TEST_ASSERT(room_quad_tag::colorIdOf(word) == color);
                    // ...and both come back out of the tag
                    TEST_ASSERT(room_quad_tag::formOf(word) == form);
                    if (form != FormEnum::NONE) {
                        TEST_ASSERT(room_quad_tag::dirOf(word) == dir);
                    }
                }
            }
        }
    }
    // The two forms are different, and so are the six directions.
    TEST_ASSERT(room_quad_tag::pack(FormEnum::DOOR, 2) != room_quad_tag::pack(FormEnum::WALL, 2));
    TEST_ASSERT(room_quad_tag::pack(FormEnum::DOOR, 2) != room_quad_tag::pack(FormEnum::DOOR, 3));
}

} // namespace

namespace test {
void testExplorerSecrets()
{
    Map::enableExtraSanityChecks(true);
    testBits();
    testFoundMask();
    testSingleFindPatches();
    testQuadTag();
}
} // namespace test
