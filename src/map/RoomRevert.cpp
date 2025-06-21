// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "RoomRevert.h"

#include "../global/AnsiOstream.h"

// <optional> and <ostream> are included via RoomRevert.h

namespace room_revert {

static std::optional<RevertPlan> build_plan_internal(AnsiOstream &os,
                                                     const Map &currentMap,
                                                     const RoomId roomId,
                                                     const Map &baseMap)
{
    const auto &thisRoom = currentMap.getRoomHandle(roomId);
    const ExternalRoomId currentExtId = thisRoom.getIdExternal();
    const auto pBefore = baseMap.findRoomHandle(currentExtId);
    if (!pBefore) {
        os << "Room " << currentExtId.value()
           << " has been added since the last save, so it cannot be reverted.\n";
        return std::nullopt;
    }

    std::optional<RevertPlan> result;
    auto &plan = result.emplace();

    const RawRoom &orig = plan.expect = pBefore.getRaw();
    // The "before" room will be modified.
    RawRoom before = orig;
    const RawRoom &after = thisRoom.getRaw();

    bool &hintUndelete = plan.hintUndelete;
    auto filterExisting = [&baseMap, &currentMap, &hintUndelete, &os](TinyRoomIdSet &set,
                                                                      const std::string_view what,
                                                                      const ExitDirEnum dir) {
        auto tmpCopy = std::exchange(set, {});
        for (const RoomId to : tmpCopy) {
            if (currentMap.findRoomHandle(to)) {
                set.insert(to);
            } else {
                const auto otherExtId = baseMap.getExternalRoomId(to).value();
                os << "Warning: Room " << otherExtId
                   << " does not exist in the current map, so the " << what << " "
                   << to_string_view(dir) << " cannot be restored.\n";
                hintUndelete = true;
            }
        }
    };

    bool &warnNoEntrances = plan.warnNoEntrances;
    for (const ExitDirEnum dir : ALL_EXITS7) {
        auto &beforeEx = before.exits[dir];
        const auto &afterEx = after.exits[dir];

        filterExisting(beforeEx.outgoing, "exit", dir);

        if ((false)) {
            filterExisting(beforeEx.incoming, "entrance", dir);
        } else {
            if (beforeEx.incoming != afterEx.incoming) {
                warnNoEntrances = true;
            }
            beforeEx.incoming = {};
        }
    }

    ChangeList &changes = plan.changes;
    for (const ExitDirEnum dir : ALL_EXITS7) {
        const auto &beforeEx = before.exits[dir];
        const auto &afterEx = after.exits[dir];
        const auto &beforeOut = beforeEx.outgoing;
        const auto &afterOut = afterEx.outgoing;
        bool addedAny = false;
        bool removedAny = false;

        // Note: adding an exit forces the existence of ExitFlagEnum::EXIT,
        // while removing the last exit can cause the removal of all ExitFlags,
        // DoorFlags, and DoorName.
        //
        // Therefore, we'll add before removing to help minimize the # of actual changes
        // that occur in the map's internal data structures.

        for (const RoomId to : beforeOut) {
            if (!afterOut.contains(to)) {
                addedAny = true;
                changes.add(exit_change_types::ModifyExitConnection{ChangeTypeEnum::Add,
                                                                    roomId,
                                                                    dir,
                                                                    to,
                                                                    WaysEnum::OneWay});
            }
        }

        for (const RoomId to : afterOut) {
            if (!beforeOut.contains(to)) {
                removedAny = true;
                changes.add(exit_change_types::ModifyExitConnection{ChangeTypeEnum::Remove,
                                                                    roomId,
                                                                    dir,
                                                                    to,
                                                                    WaysEnum::OneWay});
            }
        }

        // Note: If the exits change (above), then then the flags may differ after the change,
        // even if they're the same right now, so we have to set them again if anything changed.

#define X_SET_PROP(_Type, _Name, _OptInit) \
    if (addedAny || removedAny || beforeEx.fields._Name != afterEx.fields._Name) { \
        changes.add(exit_change_types::ModifyExitFlags{roomId, \
                                                       dir, \
                                                       beforeEx.fields._Name, \
                                                       FlagModifyModeEnum::ASSIGN}); \
    }
        XFOREACH_EXIT_PROPERTY(X_SET_PROP)
#undef X_SET_PROP
    }

#define X_SET_PROP(_Type, _Name, _OptInit) \
    if (before.fields._Name != after.fields._Name) { \
        changes.add(room_change_types::ModifyRoomFlags{roomId, \
                                                       before.fields._Name, \
                                                       FlagModifyModeEnum::ASSIGN}); \
    }
    XFOREACH_ROOM_PROPERTY(X_SET_PROP)
#undef X_SET_PROP

    if (before.server_id != after.server_id) {
        changes.add(room_change_types::SetServerId{roomId, before.server_id});
    }

    if (before.position != after.position) {
        if (currentMap.findRoomHandle(before.position)) {
            os << "Warning: The room room's old position is occupied, so it will not be moved.\n";
        } else {
            changes.add(room_change_types::TryMoveCloseTo{roomId, before.position});
        }
    }

    if (before.status != after.status) {
        switch (before.status) {
        case RoomStatusEnum::Permanent:
            changes.add(room_change_types::MakePermanent{roomId});
            break;
        case RoomStatusEnum::Temporary:
        case RoomStatusEnum::Zombie:
        default:
            os << "Warning: Room status cannot be restored (this case should not be possible).\n";
            break;
        }
    }

    return result;
}
std::optional<RevertPlan> build_plan(AnsiOstream &os,
                                     const Map &currentMap,
                                     const RoomId roomId,
                                     const Map &baseMap)
{
    try {
        return build_plan_internal(os, currentMap, roomId, baseMap);
    } catch (...) {
        os << "Error: Exception while building plan.\n";
        return std::nullopt;
    }
}

bool isRevertible(const Map &currentMap, const RoomId roomId, const Map &baseMap)
{
    const auto currentRoomHandle = currentMap.findRoomHandle(roomId);
    if (!currentRoomHandle) {
        // Room doesn't even exist in the current map, so it can't be reverted
        // based on an external ID from currentMap.
        return false;
    }
    const ExternalRoomId currentExtId = currentRoomHandle.getIdExternal();
    if (currentExtId == INVALID_EXTERNAL_ROOMID) {
        // Should not happen for a valid room handle, but good to check.
        return false;
    }
    return baseMap.findRoomHandle(currentExtId).isValid();
}

bool isRevertible(const Map &currentMap, const RoomIdSet &roomIds, const Map &baseMap)
{
    if (roomIds.empty()) {
        return false;
    }
    for (const RoomId roomId : roomIds) {
        if (isRevertible(currentMap, roomId, baseMap)) {
            return true;
        }
    }
    return false;
}

std::vector<RevertPlan> build_plan(AnsiOstream &os,
                                   const Map &currentMap,
                                   const RoomIdSet &roomIds,
                                   const Map &baseMap)
{
    std::vector<RevertPlan> plans;
    if (roomIds.empty()) {
        return plans;
    }

    for (const RoomId roomId : roomIds) {
        // Use the existing single-room build_plan function.
        // This already handles os messages for non-revertible rooms or errors.
        std::optional<RevertPlan> roomPlan = build_plan(os, currentMap, roomId, baseMap);
        if (roomPlan) {
            plans.push_back(std::move(*roomPlan));
        }
    }
    return plans;
}

} // namespace room_revert
