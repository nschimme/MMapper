#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "../global/macros.h"
#include "Changes.h"
#include "Map.h"        // Added for Map class
#include "RoomIdSet.h"  // Added for RoomIdSet
// roomid.h is included via Changes.h -> ChangeTypes.h

#include <optional>
#include <ostream>
#include <vector>       // Added for std::vector

namespace room_revert {

struct NODISCARD RevertPlan final
{
    RawRoom expect;     // what we expect to see at the end
    ChangeList changes; // changes to apply
    bool hintUndelete = false;
    bool warnNoEntrances = false;
};

// assumes the current map is a modified version of the base map
NODISCARD std::optional<RevertPlan> build_plan(AnsiOstream &os,
                                               const Map &currentMap,
                                               RoomId roomId,
                                               const Map &baseMap);

// checks if a room can be reverted (i.e. it existed in the baseMap)
NODISCARD bool isRevertible(const Map &currentMap,
                            RoomId roomId,
                            const Map &baseMap);

// checks if any room in the set can be reverted
NODISCARD bool isRevertible(const Map &currentMap,
                            const RoomIdSet &roomIds,
                            const Map &baseMap);

// builds a list of revert plans for a set of rooms
// skips rooms that cannot be reverted or for which a plan cannot be built
NODISCARD std::vector<RevertPlan> build_plan(AnsiOstream &os,
                                             const Map &currentMap,
                                             const RoomIdSet &roomIds,
                                             const Map &baseMap);

} // namespace room_revert
