// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/progresscounter.h"
#include "../global/tests.h"
#include "Change.h"
#include "ChangeTypes.h"
#include "ExplorerFogMask.h"

namespace { // anonymous

void testMask()
{
    ProgressCounter pc;
    Map map;
    const auto add = [&](const Coordinate pos, const uint32_t serverId) {
        map = map.applySingleChange(pc, Change{room_change_types::AddPermanentRoom{pos}}).map;
        const RoomId id = map.findRoomHandle(pos).getId();
        map = map.applySingleChange(pc,
                                    Change{
                                        room_change_types::SetServerId{id, ServerRoomId{serverId}}})
                  .map;
        return id;
    };
    const RoomId known = add(Coordinate{0, 0, 0}, 10);
    const RoomId hidden = add(Coordinate{4, 1, 0}, 11);
    const RoomId above = add(Coordinate{0, 0, 2}, 12);
    map = map.applySingleChange(pc,
                                Change{exit_change_types::ModifyExitConnection{ChangeTypeEnum::Add,
                                                                               known,
                                                                               ExitDirEnum::EAST,
                                                                               hidden,
                                                                               WaysEnum::TwoWay}})
              .map;

    // Inactive: nothing is fogged, so the renderer skips the mask altogether.
    TEST_ASSERT(buildExplorerFogMask(map, ExplorerView{}).empty());

    // Active but nothing computed yet: fail closed, only the current room is shown.
    const auto view = ExplorerView::makeActive(nullptr, map, known, {});
    const auto mask = buildExplorerFogMask(map, view);
    TEST_ASSERT(!mask.empty());
    TEST_ASSERT(mask.width == 5 && mask.height == 2 && mask.depth == 3);
    TEST_ASSERT(mask.at(Coordinate{0, 0, 0}) == ExplorerFogMask::SHOWN);
    // One step from the player's room through a visible exit: hinted, not shown.
    TEST_ASSERT(mask.at(Coordinate{4, 1, 0}) == ExplorerFogMask::FRONTIER);
    TEST_ASSERT(mask.at(Coordinate{0, 0, 2}) == ExplorerFogMask::HIDDEN);
    // Cells without a room hold nothing to hide, so long connections stay whole.
    TEST_ASSERT(mask.at(Coordinate{2, 1, 1}) == ExplorerFogMask::SHOWN);
    // Outside the bounds is hidden.
    TEST_ASSERT(mask.at(Coordinate{-1, 0, 0}) == ExplorerFogMask::HIDDEN);
    TEST_ASSERT(mask.at(Coordinate{0, 0, 3}) == ExplorerFogMask::HIDDEN);
    (void) above;

    // A shown base reveals its rooms; frontier rooms are hinted, never shown.
    auto base = std::make_shared<ExplorerVisibility>();
    base->visible.insert(above);
    base->frontier.insert(hidden);
    const auto view2 = ExplorerView::makeActive(base, map, std::nullopt, {});
    const auto mask2 = buildExplorerFogMask(map, view2);
    TEST_ASSERT(mask2.at(Coordinate{0, 0, 2}) == ExplorerFogMask::SHOWN);
    TEST_ASSERT(mask2.at(Coordinate{4, 1, 0}) == ExplorerFogMask::FRONTIER);
    TEST_ASSERT(mask2.at(Coordinate{0, 0, 0}) == ExplorerFogMask::HIDDEN);
}

} // namespace

namespace test {
void testExplorerFogMask()
{
    testMask();
}
} // namespace test
