// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/progresscounter.h"
#include "../global/tests.h"
#include "Change.h"
#include "ChangeTypes.h"
#include "ExplorerVisibility.h"
#include "parseevent.h"

namespace { // anonymous

struct NODISCARD Fixture final
{
    ProgressCounter pc;
    Map map;

    void apply(Change change) { map = map.applySingleChange(pc, change).map; }

    NODISCARD RoomId add(const int x, const std::optional<uint32_t> serverId = std::nullopt)
    {
        const Coordinate pos{x, 0, 0};
        apply(Change{room_change_types::AddPermanentRoom{pos}});
        const RoomId id = map.getRoomHandle(map.findRoomHandle(pos).getId()).getId();
        if (serverId) {
            apply(Change{room_change_types::SetServerId{id, ServerRoomId{*serverId}}});
        }
        return id;
    }

    // Rooms are laid out along x, so EAST/WEST connect neighbours.
    void connect(const RoomId from,
                 const ExitDirEnum dir,
                 const RoomId to,
                 const WaysEnum ways = WaysEnum::TwoWay)
    {
        apply(Change{
            exit_change_types::ModifyExitConnection{ChangeTypeEnum::Add, from, dir, to, ways}});
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

    NODISCARD ExplorerVisibility compute(const std::vector<uint32_t> &known,
                                         const DiscoveredExits &found = {},
                                         const std::optional<RoomId> current = std::nullopt,
                                         const RoomIdSet &placed = {}) const
    {
        ExplorerVisibilityInput in;
        in.map = map;
        for (const uint32_t id : known) {
            in.known.insert(ServerRoomId{id});
        }
        in.discoveredExits = found;
        in.currentRoom = current;
        in.placedRooms = placed;
        return computeExplorerVisibility(in);
    }
};

void testBasicVisibility()
{
    Fixture f;
    const RoomId known = f.add(0, 10);
    const RoomId unknown = f.add(5, 11);
    const RoomId noId = f.add(7);

    auto v = f.compute({10});
    TEST_ASSERT(v.visible.contains(known));
    TEST_ASSERT(!v.visible.contains(unknown));
    // No server ID and not placed: nothing can prove discovery, so it fails closed.
    TEST_ASSERT(!v.visible.contains(noId));

    // The same room is visible once the path machine has placed the player in it this session.
    RoomIdSet placed;
    placed.insert(noId);
    v = f.compute({10}, {}, std::nullopt, placed);
    TEST_ASSERT(v.visible.contains(noId));
    // Being placed never makes a room with a server ID visible.
    placed.insert(unknown);
    v = f.compute({10}, {}, std::nullopt, placed);
    TEST_ASSERT(!v.visible.contains(unknown));

    // The current room is always visible, whatever the data says.
    v = f.compute({}, {}, unknown);
    TEST_ASSERT(v.visible.contains(unknown));
    TEST_ASSERT(!v.visible.contains(known));
    v = f.compute({}, {}, noId);
    TEST_ASSERT(v.visible.contains(noId));

    // Nothing known, no current room: nothing at all is shown.
    v = f.compute({});
    TEST_ASSERT(v.visible.empty());
    TEST_ASSERT(v.frontier.empty());
}

void testTemporaryRooms()
{
    Fixture f;
    const RoomId permanent = f.add(0, 10);
    f.apply(
        Change{room_change_types::AddRoom2{Coordinate{1, 0, 0}, *ParseEvent::createDummyEvent()}});
    const RoomHandle temp = f.map.findRoomHandle(Coordinate{1, 0, 0});
    TEST_ASSERT(temp.exists());
    TEST_ASSERT(temp.isTemporary());
    f.connect(permanent, ExitDirEnum::EAST, temp.getId());

    // A temporary room is a mapping guess: never visible, never frontier...
    auto v = f.compute({10});
    TEST_ASSERT(v.visible.contains(permanent));
    TEST_ASSERT(!v.visible.contains(temp.getId()));
    TEST_ASSERT(!v.frontier.contains(temp.getId()));
    // ...unless it is where the player is standing.
    v = f.compute({10}, {}, temp.getId());
    TEST_ASSERT(v.visible.contains(temp.getId()));
}

void testFrontier()
{
    Fixture f;
    const RoomId a = f.add(0, 10);
    const RoomId b = f.add(1, 11);
    const RoomId c = f.add(2, 12);
    f.connect(a, ExitDirEnum::EAST, b);
    f.connect(b, ExitDirEnum::EAST, c);

    // One step beyond a visible room is frontier; two steps is simply hidden.
    auto v = f.compute({10});
    TEST_ASSERT(v.visible.contains(a));
    TEST_ASSERT(v.frontier.contains(b));
    TEST_ASSERT(!v.frontier.contains(c));
    TEST_ASSERT(!v.visible.contains(c));

    // Frontier and visible never overlap: knowing b removes it from the frontier.
    v = f.compute({10, 11});
    TEST_ASSERT(v.visible.contains(b));
    TEST_ASSERT(!v.frontier.contains(b));
    TEST_ASSERT(v.frontier.contains(c));
}

void testFrontierUsesOutgoingExitsOnly()
{
    Fixture f;
    const RoomId unknown = f.add(0, 11);
    const RoomId known = f.add(1, 10);
    // A one-way exit from an unknown room into a known one says nothing about the unknown room.
    f.connect(unknown, ExitDirEnum::EAST, known, WaysEnum::OneWay);

    const auto v = f.compute({10});
    TEST_ASSERT(v.visible.contains(known));
    TEST_ASSERT(!v.visible.contains(unknown));
    TEST_ASSERT(!v.frontier.contains(unknown));
}

void testVerticalExits()
{
    Fixture f;
    const RoomId a = f.add(0, 10);
    f.apply(Change{room_change_types::AddPermanentRoom{Coordinate{0, 0, 1}}});
    const RoomId up = f.map.findRoomHandle(Coordinate{0, 0, 1}).getId();
    f.connect(a, ExitDirEnum::UP, up);
    const auto v = f.compute({10});
    TEST_ASSERT(v.frontier.contains(up));
}

void testHiddenExitsStayClosed()
{
    Fixture f;
    const RoomId a = f.add(0, 10);
    const RoomId b = f.add(1, 11);
    f.connect(a, ExitDirEnum::EAST, b);
    f.hide(a, ExitDirEnum::EAST);
    f.hide(b, ExitDirEnum::WEST);

    const RoomHandle ra = f.map.getRoomHandle(a);
    TEST_ASSERT(ra.getExit(ExitDirEnum::EAST).exitIsExit());
    TEST_ASSERT(ra.getExit(ExitDirEnum::EAST).doorIsHidden());

    // Undiscovered: counts as no exit, so the room behind it is hidden, NOT frontier.
    DiscoveredExits none;
    TEST_ASSERT(!isEffectiveExit(f.map, ra, ExitDirEnum::EAST, none));
    auto v = f.compute({10});
    TEST_ASSERT(v.visible.contains(a));
    TEST_ASSERT(!v.visible.contains(b));
    TEST_ASSERT(!v.frontier.contains(b));

    // Found from this side: the exit is open and the room behind it becomes frontier.
    DiscoveredExits found{DiscoveredExit{ServerRoomId{10}, ExitDirEnum::EAST}};
    TEST_ASSERT(isEffectiveExit(f.map, ra, ExitDirEnum::EAST, found));
    v = f.compute({10}, found);
    TEST_ASSERT(v.frontier.contains(b));

    // Found only from the other side: the same secret door, so this side opens too.
    DiscoveredExits foundBack{DiscoveredExit{ServerRoomId{11}, ExitDirEnum::WEST}};
    TEST_ASSERT(isEffectiveExit(f.map, ra, ExitDirEnum::EAST, foundBack));
    v = f.compute({10}, foundBack);
    TEST_ASSERT(v.frontier.contains(b));

    // A found exit in some other direction or room opens nothing here.
    DiscoveredExits unrelated{DiscoveredExit{ServerRoomId{10}, ExitDirEnum::NORTH},
                              DiscoveredExit{ServerRoomId{99}, ExitDirEnum::EAST}};
    TEST_ASSERT(!isEffectiveExit(f.map, ra, ExitDirEnum::EAST, unrelated));
}

void testHiddenExitOnRoomWithoutServerId()
{
    Fixture f;
    const RoomId a = f.add(0); // no server ID
    const RoomId b = f.add(1, 11);
    f.connect(a, ExitDirEnum::EAST, b);
    f.hide(a, ExitDirEnum::EAST);
    f.hide(b, ExitDirEnum::WEST);
    RoomIdSet placed;
    placed.insert(a);
    const RoomHandle ra = f.map.getRoomHandle(a);

    // A room with no server ID has no key to record a discovery under, so its own side can
    // never be found: it stays closed.
    DiscoveredExits found{DiscoveredExit{ServerRoomId{}, ExitDirEnum::EAST}};
    TEST_ASSERT(!isEffectiveExit(f.map, ra, ExitDirEnum::EAST, found));
    auto v = f.compute({}, found, std::nullopt, placed);
    TEST_ASSERT(v.visible.contains(a));
    TEST_ASSERT(!v.frontier.contains(b));

    // It can still be opened from the other side.
    DiscoveredExits foundBack{DiscoveredExit{ServerRoomId{11}, ExitDirEnum::WEST}};
    TEST_ASSERT(isEffectiveExit(f.map, ra, ExitDirEnum::EAST, foundBack));
    v = f.compute({}, foundBack, std::nullopt, placed);
    TEST_ASSERT(v.frontier.contains(b));
}

void testViewMatchesFullComputation()
{
    // Rooms along x with a hidden door between 2 and 3, a one-way exit, and a vertical exit.
    Fixture f;
    const RoomId r0 = f.add(0, 10);
    const RoomId r1 = f.add(1, 11);
    const RoomId r2 = f.add(2, 12);
    const RoomId r3 = f.add(3, 13);
    const RoomId r4 = f.add(4, 14);
    const RoomId r5 = f.add(5); // no server ID
    f.connect(r0, ExitDirEnum::EAST, r1);
    f.connect(r1, ExitDirEnum::EAST, r2);
    f.connect(r2, ExitDirEnum::EAST, r3);
    f.connect(r3, ExitDirEnum::EAST, r4);
    f.connect(r4, ExitDirEnum::EAST, r5, WaysEnum::OneWay);
    f.hide(r2, ExitDirEnum::EAST);
    f.hide(r3, ExitDirEnum::WEST);
    const std::vector<RoomId> all{r0, r1, r2, r3, r4, r5};

    const std::vector<DiscoveredExits> finds{{},
                                             {DiscoveredExit{ServerRoomId{12}, ExitDirEnum::EAST}}};
    const std::vector<std::vector<uint32_t>> knowns{{}, {10}, {10, 11}, {11, 14}};
    std::vector<std::optional<RoomId>> currents{std::nullopt};
    for (const RoomId id : all) {
        currents.emplace_back(id);
    }

    for (const auto &found : finds) {
        for (const auto &known : knowns) {
            // The shared base is computed without a current room, as the service does.
            const auto base = std::make_shared<const ExplorerVisibility>(
                f.compute(known, found, std::nullopt));
            for (const auto &current : currents) {
                const auto expect = f.compute(known, found, current);
                const auto view = ExplorerView::makeActive(base, f.map, current, found);
                for (const RoomId id : all) {
                    TEST_ASSERT(view.shows(id) == expect.visible.contains(id));
                    TEST_ASSERT(view.isFrontier(id) == expect.frontier.contains(id));
                    TEST_ASSERT(view.isHidden(id)
                                == (!expect.visible.contains(id) && !expect.frontier.contains(id)));
                }
            }
        }
    }
}

void testViewInactiveAndUncomputed()
{
    Fixture f;
    const RoomId a = f.add(0, 10);
    const RoomId b = f.add(1, 11);
    f.connect(a, ExitDirEnum::EAST, b);

    // Inactive (the default): everything is shown and nothing is hidden or hinted.
    const ExplorerView inactive;
    TEST_ASSERT(!inactive.isActive());
    for (const RoomId id : {a, b}) {
        TEST_ASSERT(inactive.shows(id));
        TEST_ASSERT(!inactive.isFrontier(id));
        TEST_ASSERT(!inactive.isHidden(id));
    }

    // Active before the first computation: fail closed, except the room the player is in.
    const auto view = ExplorerView::makeActive(nullptr, f.map, a, {});
    TEST_ASSERT(view.isActive());
    TEST_ASSERT(view.shows(a));
    TEST_ASSERT(view.isFrontier(b)); // one step beyond the room the player stands in
    const auto none = ExplorerView::makeActive(nullptr, f.map, std::nullopt, {});
    TEST_ASSERT(none.isHidden(a));
    TEST_ASSERT(none.isHidden(b));
}
} // namespace

namespace test {
void testExplorerVisibility()
{
    Map::enableExtraSanityChecks(true);
    testBasicVisibility();
    testTemporaryRooms();
    testFrontier();
    testFrontierUsesOutgoingExitsOnly();
    testVerticalExits();
    testHiddenExitsStayClosed();
    testHiddenExitOnRoomWithoutServerId();
    testViewMatchesFullComputation();
    testViewInactiveAndUncomputed();
}
} // namespace test
