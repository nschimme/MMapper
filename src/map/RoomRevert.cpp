// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "RoomRevert.h"

#include "../global/AnsiOstream.h"
#include "Map.h"
#include "RoomIdSet.h"
#include "RawRoom.h" // For RawRoom
#include "ChangeList.h" // For ChangeList (though via Changes.h -> RoomRevert.h)
#include "ChangeTypes.h" // For change types (though via Changes.h -> RoomRevert.h)
#include "ExitDirection.h" // For ExitDirEnum, ALL_EXITS7 (likely via Map.h or RawRoom.h)
#include "Enums.h"         // For RoomStatusEnum etc (likely via Map.h or RawRoom.h)


#include <optional>
#include <ostream>
#include <string_view>
#include <utility> // For std::exchange

namespace room_revert {

// Renamed and signature changed
static std::optional<RevertPlan> build_single_room_plan_details(AnsiOstream &os,
                                                                const Map &currentMap,
                                                                const RoomId roomId,
                                                                const RawRoom &baseRoomRaw, // Base state passed in
                                                                const Map &baseMap)         // baseMap still needed for context like getExternalRoomId
{
    const auto &thisRoomHandle = currentMap.getRoomHandle(roomId);
    // ExternalRoomId currentExtId = thisRoomHandle.getIdExternal(); // Not strictly needed here if baseRoomRaw is already matched

    std::optional<RevertPlan> result;
    auto &plan = result.emplace();

    plan.expect = baseRoomRaw; // Expect to revert to baseRoomRaw
    plan.hintUndelete = false; // Initialize
    plan.warnNoEntrances = false; // Initialize

    // The "before" state for comparison is baseRoomRaw.
    // The "after" state is the current room's state.
    const RawRoom &currentRoomRaw = thisRoomHandle.getRaw();

    // Lambda to filter exits based on existence in currentMap
    // Used to ensure we don't try to restore exits to non-existent rooms.
    auto filterExisting = [&](TinyRoomIdSet &set,
                              const std::string_view what,
                              const ExitDirEnum dir) {
        auto tmpCopy = std::exchange(set, {});
        for (const RoomId to : tmpCopy) {
            if (currentMap.findRoomHandle(to)) {
                set.insert(to);
            } else {
                // Try to get External ID from baseMap for the warning message
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

    // Iterate over exits to revert them
    for (const ExitDirEnum dir : ALL_EXITS7) {
        // Exits in baseRoomRaw (what we want to restore)
        RawExit baseEx = baseRoomRaw.exits[dir]; // Make a copy to potentially modify (filter)
        // Exits in currentRoomRaw (what is currently there)
        const auto &currentEx = currentRoomRaw.exits[dir];

        // Filter outgoing exits of baseRoomRaw: only keep those leading to rooms existing in currentMap
        filterExisting(baseEx.outgoing, "exit", dir);

        // Incoming exits are not directly restored to avoid unexpected connections.
        // However, warn if they differ from what they were in baseRoomRaw.
        if (baseEx.incoming != currentEx.incoming) {
            // This warning is a bit tricky. If baseEx.incoming was filtered, this comparison might not be what's intended.
            // The original logic was: if (beforeEx.incoming != afterEx.incoming) warnNoEntrances = true; beforeEx.incoming = {};
            // We are not modifying baseEx.incoming for plan generation other than filtering.
            // The original logic implies we *don't* try to restore incoming, but *do* warn if different.
            // For now, let's warn if the *original* base incoming differs from current.
            if (baseRoomRaw.exits[dir].incoming != currentEx.incoming) {
                 plan.warnNoEntrances = true;
            }
        }

        // Compare filtered base exits with current exits
        const auto &restoredOut = baseEx.outgoing; // These are the exits we will try to restore
        const auto &currentOut = currentEx.outgoing;
        bool addedAny = false;
        bool removedAny = false;

        // Add exits that are in baseRoomRaw (filtered) but not in currentRoomRaw
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

        // Remove exits that are in currentRoomRaw but not in baseRoomRaw (filtered)
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

        // Revert exit properties if exits were changed or properties differ
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

    // Revert room properties
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
        // Check if the target position (baseRoomRaw.position) is occupied by another room in currentMap
        // that is not the room we are currently trying to move (roomId).
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
            // TryMoveCloseTo is probably not what we want if we know the exact original coords.
            // A direct SetPosition would be better if available and safe.
            // Assuming TryMoveCloseTo is the mechanism to attempt repositioning.
            changes.add(room_change_types::TryMoveCloseTo{roomId, baseRoomRaw.position});
        }
    }

    if (baseRoomRaw.status != currentRoomRaw.status) {
        switch (baseRoomRaw.status) {
        case RoomStatusEnum::Permanent:
            changes.add(room_change_types::MakePermanent{roomId});
            break;
        case RoomStatusEnum::Temporary:
        case RoomStatusEnum::Zombie: // Reverting to Temporary/Zombie might need specific actions
                                     // or might not be fully supported by simple changes.
                                     // For now, mirror original logic.
            os << "Warning: For room " << thisRoomHandle.getIdExternal().value()
               << ", its status was " << to_string_view(baseRoomRaw.status)
               << ". Reverting to Temporary or Zombie status is not fully supported and might be ignored or lead to default temporary status.\n";
            // Potentially add a change to make it temporary if that's the desired fallback.
            // changes.add(room_change_types::MakeTemporary{roomId}); // Example
            break;
        default:
            os << "Warning: For room " << thisRoomHandle.getIdExternal().value()
               << ", its status " << to_string_view(baseRoomRaw.status)
               << " cannot be restored (this case should ideally not be possible).\n";
            break;
        }
    }

    // If no changes were generated, it might mean the room is already in the base state
    // or all changes were filtered out (e.g. exits to non-existent rooms).
    // The caller (build_plan) will check if aggregated_plan_opt->changes is empty.
    return result;
}

// New build_plan function taking RoomIdSet
std::optional<RevertPlan> build_plan(AnsiOstream &os,
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
        const auto& currentRoomHandle = pCurrentRoom.value();
        const ExternalRoomId currentExtId = currentRoomHandle.getIdExternal();

        const auto pBaseRoom = baseMap.findRoomHandle(currentExtId);
        if (!pBaseRoom) {
            os << "Info: Room " << currentExtId.value()
               << " (" << currentRoomHandle.getName().toStdStringViewUtf8() << ")"
               << " from current map does not exist in the base map (or its external ID changed), so it cannot be reverted.\n";
            continue;
        }

        // Check general revertibility first (e.g. if it was added since last save)
        // This uses the isRevertible logic which is simpler and was added previously.
        // We can make this more granular if needed.
        if (!isRevertible(os, currentMap, roomId, baseMap)) {
            // isRevertible already prints a message.
            continue;
        }

        const RawRoom &baseRoomRaw = pBaseRoom.getRaw();
        std::optional<RevertPlan> single_room_plan = build_single_room_plan_details(os, currentMap, roomId, baseRoomRaw, baseMap);

        if (single_room_plan) {
            if (!aggregated_plan_opt) {
                aggregated_plan_opt.emplace();
                // For the first room, its 'expect' could be ambiguous if we revert multiple rooms.
                // The RevertPlan::expect is for a single room. For a set, it's less clear.
                // For now, let's leave 'expect' as default/empty for aggregate, or rethink its meaning.
                // The task asks to initialize with the first plan's details:
                aggregated_plan_opt->expect = single_room_plan->expect; // This will be the 'expect' of the *first* successfully processed room.
                                                                       // This might need clarification if used for multi-room direct state validation.
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


bool isRevertible(AnsiOstream &os,
                  const Map &currentMap,
                  const RoomId roomId,
                  const Map &baseMap)
{
    const auto& thisRoom = currentMap.getRoomHandle(roomId);
    const ExternalRoomId currentExtId = thisRoom.getIdExternal();
    const auto pBefore = baseMap.findRoomHandle(currentExtId);
    if (!pBefore) {
        // Message modified slightly to match the one in the new build_plan for consistency
        os << "Room " << currentExtId.value()
           << " (" << thisRoom.getName().toStdStringViewUtf8() << ")"
           << " has been added since the last save (or its external ID changed), so it cannot be reverted.\n";
        return false;
    }
    return true;
}

bool isRevertible(AnsiOstream &os,
                  const Map &currentMap,
                  const RoomIdSet &roomIds,
                  const Map &baseMap)
{
    if (roomIds.empty()) {
        // os << "Info: No rooms specified to check for revertibility.\n"; // Minor: message for empty set
        return false; // Consistent with build_plan, empty set is not revertible overall.
    }
    bool atLeastOneRevertibleExistsInCurrentMap = false;
    bool atLeastOneActuallyRevertible = false;

    for (const RoomId roomId : roomIds) {
        const auto pCurrentRoom = currentMap.findRoomHandle(roomId);
        if (pCurrentRoom) { // Check if room exists in current map first
            atLeastOneRevertibleExistsInCurrentMap = true;
            if (isRevertible(os, currentMap, roomId, baseMap)) { // Calls the single room version
                atLeastOneActuallyRevertible = true;
                // For this function's purpose, if we find one, we could return true.
                // But the original logic was to check all and print messages for all.
                // Let's stick to checking all and then deciding.
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

    if (!atLeastOneActuallyRevertible && atLeastOneRevertibleExistsInCurrentMap) { // Modified condition
        os << "Info: None of the specified rooms (that exist in the current map) are revertible.\n";
    }
    return atLeastOneActuallyRevertible;
}

} // namespace room_revert
