// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "RoomRevert.h"

#include "../global/AnsiOstream.h"
#include "Map.h"
#include "RoomIdSet.h"
#include "RawRoom.h"
#include "ChangeList.h"
#include "ChangeTypes.h"
#include "ExitDirection.h"
#include "Enums.h"

#include <optional>
#include <ostream>
#include <string_view>
#include <utility>

namespace room_revert {

static std::optional<RevertPlan> build_single_room_plan_details(AnsiOstream &os, // Changed back to reference
                                                                const Map &currentMap,
                                                                const RoomId roomId,
                                                                const RawRoom &baseRoomRaw,
                                                                const Map &baseMap)
{
    const auto &thisRoomHandle = currentMap.getRoomHandle(roomId);

    std::optional<RevertPlan> result;
    auto &plan = result.emplace();

    // plan.expect = baseRoomRaw; // Removed
    plan.hintUndelete = false;
    plan.warnNoEntrances = false;

    const RawRoom &currentRoomRaw = thisRoomHandle.getRaw();

    auto filterExisting = [&](TinyRoomIdSet &set,
                              const std::string_view what,
                              const ExitDirEnum dir) {
        auto tmpCopy = std::exchange(set, {});
        for (const RoomId to : tmpCopy) {
            if (currentMap.findRoomHandle(to)) {
                set.insert(to);
            } else {
                const auto otherExtIdOpt = baseMap.getExternalRoomId(to);
                os << "Warning: For room " << thisRoomHandle.getIdExternal().value()
                   << ", the target room of " << what << " " << to_string_view(dir);
                if (otherExtIdOpt) {
                    os << " (Original External ID: " << otherExtIdOpt.value().value() << ")";
                } else {
                    os << " (Internal ID: " << to.value() << ")";
                }
                os << " does not exist in the current map, so this connection cannot be restored.\n";
                plan.hintUndelete = true;
            }
        }
    };

    ChangeList &changes = plan.changes;

    for (const ExitDirEnum dir : ALL_EXITS7) {
        RawExit baseEx = baseRoomRaw.exits[dir];
        const auto &currentEx = currentRoomRaw.exits[dir];

        filterExisting(baseEx.outgoing, "exit", dir);

        if (baseRoomRaw.exits[dir].incoming != currentEx.incoming) {
             plan.warnNoEntrances = true;
        }

        const auto &restoredOut = baseEx.outgoing;
        const auto &currentOut = currentEx.outgoing;
        bool addedAny = false;
        bool removedAny = false;

        for (const RoomId to : restoredOut) {
            if (!currentOut.contains(to)) {
                addedAny = true;
                changes.add(exit_change_types::ModifyExitConnection{ChangeTypeEnum::Add,
                                                                    roomId,
                                                                    dir,
                                                                    to,
                                                                    WaysEnum::OneWay});
            }
        }

        for (const RoomId to : currentOut) {
            if (!restoredOut.contains(to)) {
                removedAny = true;
                changes.add(exit_change_types::ModifyExitConnection{ChangeTypeEnum::Remove,
                                                                    roomId,
                                                                    dir,
                                                                    to,
                                                                    WaysEnum::OneWay});
            }
        }

#define X_SET_PROP(_Type, _Name, _OptInit) \
    if (addedAny || removedAny || baseEx.fields._Name != currentEx.fields._Name) { \
        changes.add(exit_change_types::ModifyExitFlags{roomId, \
                                                       dir, \
                                                       baseEx.fields._Name, \
                                                       FlagModifyModeEnum::ASSIGN}); \
    }
        XFOREACH_EXIT_PROPERTY(X_SET_PROP)
#undef X_SET_PROP
    }

#define X_SET_PROP(_Type, _Name, _OptInit) \
    if (baseRoomRaw.fields._Name != currentRoomRaw.fields._Name) { \
        changes.add(room_change_types::ModifyRoomFlags{roomId, \
                                                       baseRoomRaw.fields._Name, \
                                                       FlagModifyModeEnum::ASSIGN}); \
    }
    XFOREACH_ROOM_PROPERTY(X_SET_PROP)
#undef X_SET_PROP

    if (baseRoomRaw.server_id != currentRoomRaw.server_id) {
        changes.add(room_change_types::SetServerId{roomId, baseRoomRaw.server_id});
    }

    if (baseRoomRaw.position != currentRoomRaw.position) {
        bool positionOccupied = false;
        if (const auto& occupyingRoomHandle = currentMap.findRoomHandle(baseRoomRaw.position)) {
            if (occupyingRoomHandle.getId() != roomId) {
                positionOccupied = true;
            }
        }

        if (positionOccupied) {
            os << "Warning: For room " << thisRoomHandle.getIdExternal().value()
               << ", its original position (" << baseRoomRaw.position.x << "," << baseRoomRaw.position.y << "," << baseRoomRaw.position.z
               << ") is now occupied by another room. It will not be moved back to that specific position.\n";
        } else {
            changes.add(room_change_types::TryMoveCloseTo{roomId, baseRoomRaw.position});
        }
    }

    if (baseRoomRaw.status != currentRoomRaw.status) {
        switch (baseRoomRaw.status) {
        case RoomStatusEnum::Permanent:
            changes.add(room_change_types::MakePermanent{roomId});
            break;
        case RoomStatusEnum::Temporary:
        case RoomStatusEnum::Zombie:
            os << "Warning: For room " << thisRoomHandle.getIdExternal().value()
               << ", its status was " << to_string_view(baseRoomRaw.status)
               << ". Reverting to Temporary or Zombie status is not fully supported and might be ignored or lead to default temporary status.\n";
            break;
        default:
            os << "Warning: For room " << thisRoomHandle.getIdExternal().value()
               << ", its status " << to_string_view(baseRoomRaw.status)
               << " cannot be restored (this case should ideally not be possible).\n";
            break;
        }
    }

    return result;
}

std::optional<RevertPlan> build_plan(AnsiOstream &os, // Changed back to reference
                                     const Map &currentMap,
                                     const RoomIdSet &roomIds,
                                     const Map &baseMap)
{
    if (roomIds.empty()) {
        os << "Info: No rooms specified for revert plan.\n";
        return std::nullopt;
    }

    std::optional<RevertPlan> aggregated_plan_opt;

    for (const RoomId roomId : roomIds) {
        const auto pCurrentRoom = currentMap.findRoomHandle(roomId);
        if (!pCurrentRoom) {
            os << "Warning: RoomId " << roomId.value()
               << " does not exist in the current map and will be skipped for revert.\n";
            continue;
        }
        // const auto& currentRoomHandle = pCurrentRoom.value(); // Not used directly after this
        // const ExternalRoomId currentExtId = currentRoomHandle.getIdExternal(); // Not used directly

        // const auto pBaseRoom = baseMap.findRoomHandle(currentExtId); // Done by isRevertible

        if (!isRevertible(os, currentMap, roomId, baseMap)) {
            continue;
        }

        // pBaseRoom is guaranteed to be valid here due to isRevertible check.
        const auto pBaseRoom = baseMap.findRoomHandle(pCurrentRoom.value().getIdExternal());
        const RawRoom &baseRoomRaw = pBaseRoom.getRaw();
        std::optional<RevertPlan> single_room_plan = build_single_room_plan_details(os, currentMap, roomId, baseRoomRaw, baseMap);

        if (single_room_plan) {
            if (!aggregated_plan_opt) {
                aggregated_plan_opt.emplace();
                // aggregated_plan_opt->expect = single_room_plan->expect; // Removed
            }
            aggregated_plan_opt->changes.append(single_room_plan->changes);
            aggregated_plan_opt->hintUndelete = aggregated_plan_opt->hintUndelete || single_room_plan->hintUndelete;
            aggregated_plan_opt->warnNoEntrances = aggregated_plan_opt->warnNoEntrances || single_room_plan->warnNoEntrances;
        }
    }

    if (!aggregated_plan_opt || aggregated_plan_opt->changes.empty()) {
        os << "Info: No revertible changes found for the specified room(s).\n";
        return std::nullopt;
    }

    return aggregated_plan_opt;
}


NODISCARD bool isRevertible(AnsiOstream &os, // Changed back to reference
                            const Map &currentMap,
                            RoomId roomId,
                            const Map &baseMap)
{
    const auto& thisRoom = currentMap.getRoomHandle(roomId);
    const ExternalRoomId currentExtId = thisRoom.getIdExternal();
    const auto pBefore = baseMap.findRoomHandle(currentExtId);
    if (!pBefore) {
        os << "Room " << currentExtId.value()
           << " (" << thisRoom.getName().toStdStringViewUtf8() << ")"
           << " has been added since the last save (or its external ID changed), so it cannot be reverted.\n";
        return false;
    }
    return true;
}

NODISCARD bool isRevertible(AnsiOstream &os, // Changed back to reference
                            const Map &currentMap,
                            const RoomIdSet &roomIds,
                            const Map &baseMap)
{
    if (roomIds.empty()) {
        return false;
    }
    bool atLeastOneRevertibleExistsInCurrentMap = false;
    bool atLeastOneActuallyRevertible = false;

    for (const RoomId roomId : roomIds) {
        const auto pCurrentRoom = currentMap.findRoomHandle(roomId);
        if (pCurrentRoom) {
            atLeastOneRevertibleExistsInCurrentMap = true;
            if (isRevertible(os, currentMap, roomId, baseMap)) {
                atLeastOneActuallyRevertible = true;
            }
        } else {
            os << "Warning: RoomId " << roomId.value()
               << " does not exist in the current map and cannot be checked for revertibility.\n";
        }
    }

    if (!atLeastOneRevertibleExistsInCurrentMap && !roomIds.empty()) {
         os << "Info: None of the specified RoomIds exist in the current map.\n";
         return false;
    }

    if (!atLeastOneActuallyRevertible && atLeastOneRevertibleExistsInCurrentMap) {
        os << "Info: None of the specified rooms (that exist in the current map) are revertible.\n";
    }
    return atLeastOneActuallyRevertible;
}

} // namespace room_revert
