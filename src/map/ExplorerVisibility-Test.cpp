// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/progresscounter.h"
#include "../global/tests.h"
#include "Change.h"
#include "ChangeTypes.h"
#include "ExplorerVisibility.h"
#include "parseevent.h"

#include <set>

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

NODISCARD bool isBlankExit(const RawExit &exit)
{
    return !exit.exitIsExit() && !exit.exitIsDoor() && !exit.doorIsHidden()
           && exit.getOutgoingSet().empty() && exit.getIncomingSet().empty();
}

NODISCARD bool isConsistent(const Map &map)
{
    try {
        ProgressCounter pc;
        map.checkConsistency(pc);
        return true;
    } catch (...) {
        return false;
    }
}

void testHideUndiscoveredExits()
{
    Fixture f;
    const RoomId a = f.add(0, 10);
    const RoomId b = f.add(1, 11);
    const RoomId c = f.add(2, 12);
    f.connect(a, ExitDirEnum::EAST, b);
    f.connect(b, ExitDirEnum::EAST, c);
    // An ordinary (not hidden) door between a and b must always survive.
    f.apply(Change{exit_change_types::SetExitFlags{FlagChangeEnum::Add,
                                                   a,
                                                   ExitDirEnum::EAST,
                                                   ExitFlags{ExitFlagEnum::DOOR}}});
    // The secret door between b and c, hidden from both sides.
    f.hide(b, ExitDirEnum::EAST);
    f.hide(c, ExitDirEnum::WEST);
    TEST_ASSERT(isConsistent(f.map));

    // Nothing found: both sides of the secret door become plain walls, and nothing else changes.
    const Map hidden = hideUndiscoveredExits(f.map, {});
    TEST_ASSERT(!hidden.isSamePointer(f.map));
    TEST_ASSERT(isBlankExit(hidden.getRoomHandle(b).getExit(ExitDirEnum::EAST)));
    TEST_ASSERT(isBlankExit(hidden.getRoomHandle(c).getExit(ExitDirEnum::WEST)));
    const RawExit &ordinary = hidden.getRoomHandle(a).getExit(ExitDirEnum::EAST);
    TEST_ASSERT(ordinary.exitIsExit() && ordinary.exitIsDoor() && !ordinary.doorIsHidden());
    TEST_ASSERT(ordinary.getOutgoingSet().contains(b));
    TEST_ASSERT(hidden.getRoomHandle(b).getExit(ExitDirEnum::WEST).getOutgoingSet().contains(a));
    TEST_ASSERT(isConsistent(hidden));
    // The input map is immutable: the secret is still there for whoever may see it.
    TEST_ASSERT(f.map.getRoomHandle(b).getExit(ExitDirEnum::EAST).doorIsHidden());

    // The blanked map agrees with the policy's own definition for every room and direction.
    for (const RoomId id : {a, b, c}) {
        for (const ExitDirEnum dir : ALL_EXITS_NESWUD) {
            const RoomHandle original = f.map.getRoomHandle(id);
            TEST_ASSERT(hidden.getRoomHandle(id).getExit(dir).exitIsExit()
                        == isEffectiveExit(f.map, original, dir, {}));
        }
    }

    // Found from either side: both sides of the same door come through unchanged, which is
    // nothing to hide at all.
    for (const auto &found : {DiscoveredExit{ServerRoomId{11}, ExitDirEnum::EAST},
                              DiscoveredExit{ServerRoomId{12}, ExitDirEnum::WEST}}) {
        const Map shown = hideUndiscoveredExits(f.map, {found});
        TEST_ASSERT(shown.isSamePointer(f.map));
    }

    // A find in some other room or direction does not open this door.
    const Map stillHidden
        = hideUndiscoveredExits(f.map,
                                {DiscoveredExit{ServerRoomId{11}, ExitDirEnum::NORTH},
                                 DiscoveredExit{ServerRoomId{99}, ExitDirEnum::EAST}});
    TEST_ASSERT(isBlankExit(stillHidden.getRoomHandle(b).getExit(ExitDirEnum::EAST)));
    TEST_ASSERT(isBlankExit(stillHidden.getRoomHandle(c).getExit(ExitDirEnum::WEST)));

    // A map with no secret exits is returned as is.
    Fixture plain;
    const RoomId p = plain.add(0, 1);
    const RoomId q = plain.add(1, 2);
    plain.connect(p, ExitDirEnum::EAST, q);
    TEST_ASSERT(hideUndiscoveredExits(plain.map, {}).isSamePointer(plain.map));
}

void testMapForExplorer()
{
    Fixture f;
    const RoomId a = f.add(0, 10);
    const RoomId b = f.add(1, 11);
    f.connect(a, ExitDirEnum::EAST, b);
    f.hide(a, ExitDirEnum::EAST);
    f.hide(b, ExitDirEnum::WEST);

    // Inactive (no snapshot of finds): the map is meshed unchanged.
    TEST_ASSERT(mapForExplorer(f.map, nullptr).isSamePointer(f.map));
    // Active with no finds: the secret door is a wall.
    const auto none = std::make_shared<const DiscoveredExits>();
    const Map hidden = mapForExplorer(f.map, none);
    TEST_ASSERT(isBlankExit(hidden.getRoomHandle(a).getExit(ExitDirEnum::EAST)));
    // Active with the find: it comes through.
    const auto found = std::make_shared<const DiscoveredExits>(
        DiscoveredExits{DiscoveredExit{ServerRoomId{10}, ExitDirEnum::EAST}});
    TEST_ASSERT(mapForExplorer(f.map, found).isSamePointer(f.map));
}

// "Export Base Map" keeps what is reachable from the seed rooms without hidden exits, and
// removes the rest. Explore mode reuses that walk, so its result is pinned here.
constexpr uint32_t COSY_ROOM = 4831075; // a seed of the base map

void testBaseMapExport()
{
    {
        Fixture f;
        const RoomId seed = f.add(0, COSY_ROOM);
        const RoomId open = f.add(1, 21);
        const RoomId open2 = f.add(2, 22);
        const RoomId secret = f.add(3, 23);
        const RoomId behind = f.add(4, 24);
        const RoomId island = f.add(6, 25);
        f.connect(seed, ExitDirEnum::EAST, open);
        f.connect(open, ExitDirEnum::EAST, open2);
        f.connect(open2, ExitDirEnum::EAST, secret);
        f.hide(open2, ExitDirEnum::EAST);
        f.connect(secret, ExitDirEnum::EAST, behind);
        (void) island;

        ProgressCounter pc;
        const Map base = f.map.filterBaseMap(pc);
        for (const int x : {0, 1, 2}) {
            TEST_ASSERT(base.findRoomHandle(Coordinate{x, 0, 0}).exists());
        }
        // Behind the secret door, and not connected at all.
        for (const int x : {3, 4, 6}) {
            TEST_ASSERT(!base.findRoomHandle(Coordinate{x, 0, 0}).exists());
        }
        // The hidden exit itself is gone from the room that stays.
        const RoomHandle last = base.findRoomHandle(Coordinate{2, 0, 0});
        TEST_ASSERT(last.getExit(ExitDirEnum::EAST).getOutgoingSet().empty());
        TEST_ASSERT(!last.getExit(ExitDirEnum::EAST).doorIsHidden());
    }

    {
        // Without any seed room the export leaves the map as it is.
        Fixture f;
        const RoomId a = f.add(0, 21);
        const RoomId b = f.add(1, 22);
        f.connect(a, ExitDirEnum::EAST, b);
        ProgressCounter pc;
        const Map base = f.map.filterBaseMap(pc);
        TEST_ASSERT(base.findRoomHandle(Coordinate{0, 0, 0}).exists());
        TEST_ASSERT(base.findRoomHandle(Coordinate{1, 0, 0}).exists());
    }
}

void testBaseMapIsVisibleByDefault()
{
    Fixture f;
    const RoomId seed = f.add(0, COSY_ROOM);
    const RoomId open = f.add(1, 21);
    const RoomId secret = f.add(2, 22);
    const RoomId behind = f.add(3, 23);
    const RoomId noId = f.add(5);
    const RoomId island = f.add(7, 25);
    f.connect(seed, ExitDirEnum::EAST, open);
    f.connect(open, ExitDirEnum::EAST, secret);
    f.hide(open, ExitDirEnum::EAST);
    f.connect(secret, ExitDirEnum::EAST, behind);
    f.apply(Change{room_change_types::AddPermanentRoom{Coordinate{0, 0, 1}}});
    const RoomId up = f.map.findRoomHandle(Coordinate{0, 0, 1}).getId();
    f.connect(seed, ExitDirEnum::UP, up);
    f.connect(seed, ExitDirEnum::DOWN, noId);
    f.apply(
        Change{room_change_types::AddRoom2{Coordinate{9, 0, 0}, *ParseEvent::createDummyEvent()}});
    const RoomId temp = f.map.findRoomHandle(Coordinate{9, 0, 0}).getId();
    f.connect(seed, ExitDirEnum::NORTH, temp);

    // Nothing is known, yet everything not behind a secret is shown, with or without a server
    // ID. A secret hides what is behind it, and a room nothing leads to is not "not hidden".
    auto v = f.compute({});
    TEST_ASSERT(v.baseMapFound);
    for (const RoomId id : {seed, open, up, noId}) {
        TEST_ASSERT(v.visible.contains(id));
    }
    for (const RoomId id : {secret, behind, island}) {
        TEST_ASSERT(!v.visible.contains(id));
    }
    // Temporary rooms are guesses, whatever leads to them.
    TEST_ASSERT(!v.visible.contains(temp));
    TEST_ASSERT(!v.frontier.contains(secret));

    // A secret found from its own side opens everything behind it...
    DiscoveredExits found{DiscoveredExit{ServerRoomId{21}, ExitDirEnum::EAST}};
    v = f.compute({}, found);
    TEST_ASSERT(v.visible.contains(secret));
    TEST_ASSERT(v.visible.contains(behind));
    TEST_ASSERT(!v.visible.contains(island));

    // ...and so does finding the other side of the same door.
    DiscoveredExits foundBack{DiscoveredExit{ServerRoomId{22}, ExitDirEnum::WEST}};
    v = f.compute({}, foundBack);
    TEST_ASSERT(v.visible.contains(secret));
    TEST_ASSERT(v.visible.contains(behind));

    // Knowing a room is still enough, behind a secret or on an island.
    v = f.compute({23, 25});
    TEST_ASSERT(v.visible.contains(behind));
    TEST_ASSERT(v.visible.contains(island));
    TEST_ASSERT(!v.visible.contains(secret));
}

void testNoSeedRoomsFallBackToKnownRooms()
{
    Fixture f;
    const RoomId a = f.add(0, 10);
    const RoomId b = f.add(1, 11);
    f.connect(a, ExitDirEnum::EAST, b);

    // A map without any seed room (a partial user map) never shows more than before.
    const auto v = f.compute({10});
    TEST_ASSERT(!v.baseMapFound);
    TEST_ASSERT(v.visible.contains(a));
    TEST_ASSERT(!v.visible.contains(b));
}

void testUndiscoveredBase()
{
    Fixture f;
    const RoomId seed = f.add(0, COSY_ROOM);
    const RoomId open = f.add(1, 21);
    const RoomId secret = f.add(2, 22);
    const RoomId noId = f.add(5);
    f.connect(seed, ExitDirEnum::EAST, open);
    f.connect(open, ExitDirEnum::EAST, secret);
    f.hide(open, ExitDirEnum::EAST);
    f.connect(seed, ExitDirEnum::DOWN, noId);
    f.apply(
        Change{room_change_types::AddRoom2{Coordinate{9, 0, 0}, *ParseEvent::createDummyEvent()}});
    const RoomId temp = f.map.findRoomHandle(Coordinate{9, 0, 0}).getId();
    f.connect(seed, ExitDirEnum::NORTH, temp);

    // Shown only because they are not hidden: inside `visible`, and never secret or temporary.
    auto v = f.compute({});
    for (const RoomId id : {seed, open, noId}) {
        TEST_ASSERT(v.undiscoveredBase.contains(id));
        TEST_ASSERT(v.visible.contains(id));
    }
    for (const RoomId id : {secret, temp}) {
        TEST_ASSERT(!v.undiscoveredBase.contains(id));
    }
    for (const RoomId id : v.undiscoveredBase) {
        TEST_ASSERT(v.visible.contains(id));
    }

    // A discovered room drops out of the set, and stays visible.
    v = f.compute({21});
    TEST_ASSERT(!v.undiscoveredBase.contains(open));
    TEST_ASSERT(v.visible.contains(open));
    TEST_ASSERT(v.undiscoveredBase.contains(seed));

    // A room without a server ID is discovered once the player was placed in it.
    RoomIdSet placed;
    placed.insert(noId);
    v = f.compute({}, {}, std::nullopt, placed);
    TEST_ASSERT(!v.undiscoveredBase.contains(noId));
    TEST_ASSERT(v.visible.contains(noId));

    // The current room is the player's: never undiscovered.
    v = f.compute({}, {}, seed);
    TEST_ASSERT(v.visible.contains(seed));
    TEST_ASSERT(!v.undiscoveredBase.contains(seed));

    // The view answers the same way, and the player's room is an overlay on top of the base.
    const auto base = std::make_shared<const ExplorerVisibility>(f.compute({}));
    const DiscoveredExits none;
    const auto view = ExplorerView::makeActive(base, f.map, std::nullopt, none);
    TEST_ASSERT(view.isUndiscoveredBase(seed));
    TEST_ASSERT(view.shows(seed));
    TEST_ASSERT(!view.isUndiscoveredBase(secret));
    TEST_ASSERT(!ExplorerView::makeActive(base, f.map, seed, none).isUndiscoveredBase(seed));
    TEST_ASSERT(!ExplorerView{}.isUndiscoveredBase(seed));
}

void testExitsChangeOnlyReveals()
{
    const auto set = [](std::initializer_list<DiscoveredExit> l) {
        return std::make_shared<const DiscoveredExits>(l);
    };
    const DiscoveredExit a{ServerRoomId{10}, ExitDirEnum::EAST};
    const DiscoveredExit b{ServerRoomId{11}, ExitDirEnum::WEST};
    const DiscoveredExit c{ServerRoomId{12}, ExitDirEnum::NORTH};
    const std::shared_ptr<const DiscoveredExits> none;

    // A found exit shows more: the old meshes only hide more, so they may keep drawing.
    TEST_ASSERT(exploreExitsOnlyReveal(set({a}), set({a, b})));
    TEST_ASSERT(exploreExitsOnlyReveal(set({}), set({a})));
    TEST_ASSERT(exploreExitsOnlyReveal(set({a, b}), set({a, b}))); // nothing changed
    // Leaving Explore shows everything.
    TEST_ASSERT(exploreExitsOnlyReveal(set({a}), none));
    TEST_ASSERT(exploreExitsOnlyReveal(none, none));

    // Anything that can hide a secret must drop the meshes at once, or they would show it.
    TEST_ASSERT(!exploreExitsOnlyReveal(none, set({}))); // starting to explore
    TEST_ASSERT(!exploreExitsOnlyReveal(none, set({a})));
    TEST_ASSERT(!exploreExitsOnlyReveal(set({a, b}), set({a})));    // lost an entry
    TEST_ASSERT(!exploreExitsOnlyReveal(set({a}), set({b, c})));    // another character
    TEST_ASSERT(!exploreExitsOnlyReveal(set({a, b}), set({b, c}))); // gained one, lost one
}

void testConnectionCulling()
{
    Fixture f;
    const RoomId a = f.add(0, 10);
    const RoomId b = f.add(1, 11);
    const RoomId c = f.add(2, 12);
    const RoomId d = f.add(3, 13);

    RoomIdSet visible;
    visible.insert(a);
    visible.insert(b);

    // Two shown rooms: the connection is meshed, whichever side it is asked from.
    TEST_ASSERT(exploreConnectionShown(&visible, a, b));
    TEST_ASSERT(exploreConnectionShown(&visible, b, a));
    // Two hidden rooms: nothing may show where they are, even though the fog mask would keep
    // the empty cells between them shown.
    TEST_ASSERT(!exploreConnectionShown(&visible, c, d));
    // One of each: not drawn either, from either side.
    TEST_ASSERT(!exploreConnectionShown(&visible, a, c));
    TEST_ASSERT(!exploreConnectionShown(&visible, c, a));
    // A connection to the room itself (a loop) follows that room.
    TEST_ASSERT(exploreConnectionShown(&visible, a, a));
    TEST_ASSERT(!exploreConnectionShown(&visible, c, c));

    // A room, as when an exit has no known target, follows its source.
    TEST_ASSERT(exploreRoomShown(&visible, a));
    TEST_ASSERT(!exploreRoomShown(&visible, c));

    // Not exploring (null): nothing is culled.
    TEST_ASSERT(exploreConnectionShown(nullptr, c, d));
    TEST_ASSERT(exploreRoomShown(nullptr, c));
    // Exploring with nothing computed yet (empty): everything is culled.
    const RoomIdSet none;
    TEST_ASSERT(!exploreConnectionShown(&none, a, b));
    TEST_ASSERT(!exploreRoomShown(&none, a));

    // The base visibility of a real computation is what the drawer is given, and undiscovered
    // base-map rooms are in it, so a line from a discovered to an undiscovered room is drawn.
    Fixture g;
    const RoomId seed = g.add(0, COSY_ROOM);
    const RoomId open = g.add(1, 21);
    const RoomId secret = g.add(2, 22);
    const RoomId behind = g.add(3, 23);
    g.connect(seed, ExitDirEnum::EAST, open);
    g.connect(open, ExitDirEnum::EAST, secret);
    g.hide(open, ExitDirEnum::EAST);
    g.connect(secret, ExitDirEnum::EAST, behind);
    const auto v = g.compute({21});
    TEST_ASSERT(exploreConnectionShown(&v.visible, seed, open));
    TEST_ASSERT(!exploreConnectionShown(&v.visible, open, secret));
    TEST_ASSERT(!exploreConnectionShown(&v.visible, secret, behind));
}

void testVisibleOnlyGrows()
{
    Fixture f;
    const RoomId a = f.add(0, 10);
    const RoomId b = f.add(1, 11);
    const RoomId c = f.add(2, 12);
    const auto set = [](std::initializer_list<RoomId> ids) {
        auto s = std::make_shared<RoomIdSet>();
        for (const RoomId id : ids) {
            s->insert(id);
        }
        return std::shared_ptr<const RoomIdSet>{s};
    };
    const std::shared_ptr<const RoomIdSet> none;

    // More rooms shown: the old meshes only hide more, so they may keep drawing.
    TEST_ASSERT(exploreVisibleOnlyGrows(set({a}), set({a, b})));
    TEST_ASSERT(exploreVisibleOnlyGrows(set({}), set({a})));
    TEST_ASSERT(exploreVisibleOnlyGrows(set({a, b}), set({a, b})));
    // Leaving Explore shows everything.
    TEST_ASSERT(exploreVisibleOnlyGrows(set({a}), none));
    TEST_ASSERT(exploreVisibleOnlyGrows(none, none));

    // Anything that could hide a room must drop the meshes at once.
    TEST_ASSERT(!exploreVisibleOnlyGrows(none, set({}))); // starting to explore
    TEST_ASSERT(!exploreVisibleOnlyGrows(none, set({a})));
    TEST_ASSERT(!exploreVisibleOnlyGrows(set({a, b}), set({a})));    // lost one
    TEST_ASSERT(!exploreVisibleOnlyGrows(set({a}), set({b, c})));    // another character
    TEST_ASSERT(!exploreVisibleOnlyGrows(set({a, b}), set({b, c}))); // gained one, lost one
    TEST_ASSERT(!exploreVisibleOnlyGrows(set({a}), set({})));        // not computed yet
}

void testOnlyServerIdBackfill()
{
    const RoomId room{1};
    const RoomId other{2};
    ChangeList mixed;
    mixed.add(room_change_types::AddPermanentRoom{Coordinate{0, 0, 0}});
    mixed.add(room_change_types::SetServerId{room, ServerRoomId{10}});
    mixed.add(room_change_types::MakePermanent{room});
    mixed.add(exit_change_types::SetExitFlags{FlagChangeEnum::Set,
                                              room,
                                              ExitDirEnum::EAST,
                                              ExitFlags{ExitFlagEnum::DOOR}});
    mixed.add(exit_change_types::SetDoorFlags{FlagChangeEnum::Set,
                                              room,
                                              ExitDirEnum::EAST,
                                              DoorFlags{DoorFlagEnum::HIDDEN}});
    mixed.add(room_change_types::SetServerId{other, ServerRoomId{11}});
    mixed.add(room_change_types::RemoveRoom{other});

    // Only the backfill of server IDs gets through, in order; everything else is dropped.
    const ChangeList kept = onlyServerIdBackfill(mixed);
    TEST_ASSERT(kept.getChanges().size() == 2);
    for (const Change &change : kept.getChanges()) {
        TEST_ASSERT(change.isSetServerId());
    }

    // Nothing to backfill means nothing to apply.
    ChangeList none;
    none.add(room_change_types::MakePermanent{room});
    TEST_ASSERT(onlyServerIdBackfill(none).empty());
    TEST_ASSERT(onlyServerIdBackfill(ChangeList{}).empty());
}

void testInfomarkCells()
{
    const auto cells = [](const Coordinate a, const Coordinate b, const bool text) {
        const auto r = infomarkCells(a, b, text);
        TEST_ASSERT(r.has_value());
        return std::set<Coordinate>(r->begin(), r->end());
    };

    // A label covers its anchor cell only, on its own layer, with floor division for negatives.
    TEST_ASSERT((cells(Coordinate{250, 130, 2}, Coordinate{0, 0, 0}, true)
                 == std::set<Coordinate>{Coordinate{2, 1, 2}}));
    TEST_ASSERT((cells(Coordinate{-1, -100, 0}, Coordinate{0, 0, 0}, true)
                 == std::set<Coordinate>{Coordinate{-1, -1, 0}}));

    // A line covers every cell along it.
    TEST_ASSERT((cells(Coordinate{50, 50, 0}, Coordinate{350, 50, 0}, false)
                 == std::set<Coordinate>{Coordinate{0, 0, 0},
                                         Coordinate{1, 0, 0},
                                         Coordinate{2, 0, 0},
                                         Coordinate{3, 0, 0}}));
    const auto diagonal = cells(Coordinate{50, 50, 0}, Coordinate{250, 250, 0}, false);
    for (const Coordinate c : {Coordinate{0, 0, 0}, Coordinate{1, 1, 0}, Coordinate{2, 2, 0}}) {
        TEST_ASSERT(diagonal.count(c) == 1);
    }
    TEST_ASSERT((cells(Coordinate{50, 50, 0}, Coordinate{50, 50, 0}, false).size() == 1));
    TEST_ASSERT((cells(Coordinate{350, 50, 0}, Coordinate{50, 50, 0}, false).size() == 4));

    // Too long to check: the caller must treat it as hidden.
    TEST_ASSERT(!infomarkCells(Coordinate{0, 0, 0}, Coordinate{100000000, 0, 0}, false));
}

void testInfomarkShown()
{
    Fixture f;
    const RoomId shown = f.add(0, 10);
    const RoomId hidden = f.add(2, 11); // x = 1 is an empty cell
    RoomIdSet visible;
    visible.insert(shown);
    const auto ok =
        [&](const RoomIdSet *set, const Coordinate a, const Coordinate b, const bool text) {
            return exploreInfomarkShown(set, f.map, a, b, text);
        };
    const Coordinate overShown{50, 50, 0};
    const Coordinate overEmpty{150, 50, 0};
    const Coordinate overHidden{250, 50, 0};
    const Coordinate beyond{350, 50, 0}; // empty cell past the hidden room
    (void) hidden;

    // A label follows the room under its anchor; an empty cell has nothing to hide.
    TEST_ASSERT(ok(&visible, overShown, overShown, true));
    TEST_ASSERT(ok(&visible, overEmpty, overEmpty, true));
    TEST_ASSERT(!ok(&visible, overHidden, overHidden, true));
    // Another layer holds no room there.
    TEST_ASSERT(ok(&visible, Coordinate{250, 50, 1}, Coordinate{250, 50, 1}, true));

    // A line or arrow is dropped when any cell along it holds a hidden room: an endpoint...
    TEST_ASSERT(ok(&visible, overShown, overEmpty, false));
    TEST_ASSERT(!ok(&visible, overShown, overHidden, false));
    TEST_ASSERT(!ok(&visible, overHidden, overShown, false));
    // ...or a room in the middle, with both ends over empty cells.
    TEST_ASSERT(!ok(&visible, overEmpty, beyond, false));
    TEST_ASSERT(ok(&visible, Coordinate{-150, 50, 0}, overEmpty, false));

    // Not exploring (null) hides nothing. Exploring with nothing computed (empty) hides every
    // mark, even one over an empty cell.
    const RoomIdSet none;
    TEST_ASSERT(ok(nullptr, overHidden, overHidden, true));
    TEST_ASSERT(ok(nullptr, overShown, beyond, false));
    TEST_ASSERT(!ok(&none, overEmpty, overEmpty, true));
    TEST_ASSERT(!ok(&none, overShown, overShown, true));
    // A mark too long to check is hidden while exploring, never when not.
    TEST_ASSERT(!ok(&visible, Coordinate{0, 0, 0}, Coordinate{100000000, 0, 0}, false));
    TEST_ASSERT(ok(nullptr, Coordinate{0, 0, 0}, Coordinate{100000000, 0, 0}, false));
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
    testHideUndiscoveredExits();
    testMapForExplorer();
    testBaseMapExport();
    testBaseMapIsVisibleByDefault();
    testNoSeedRoomsFallBackToKnownRooms();
    testUndiscoveredBase();
    testExitsChangeOnlyReveals();
    testInfomarkCells();
    testInfomarkShown();
    testOnlyServerIdBackfill();
    testConnectionCulling();
    testVisibleOnlyGrows();
}
} // namespace test
