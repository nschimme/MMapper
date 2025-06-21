// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include "patheventcontext.h"
#include "../map/Change.h"      // For Change class itself and its methods like isRemoveRoom()
#include "../map/ChangeTypes.h" // For room_change_types::MakePermanent, ::RemoveRoom

// Using mmapper namespace for types defined in patheventcontext.h
namespace mmapper {

// Implementation for addTrackedChange
void PathEventContext::addTrackedChange(const Change &change) {
    // Helper to get RoomId from MakePermanent or RemoveRoom changes
    auto getRoomIdFromLifecycleChange = [](const Change &c) -> std::optional<RoomId> {
        if (const auto* mp = std::get_if<room_change_types::MakePermanent>(&change.getVariant())) {
            return mp->room;
        }
        if (const auto* rr = std::get_if<room_change_types::RemoveRoom>(&change.getVariant())) {
            return rr->room;
        }
        // A bit of a workaround to access the variant directly if isRemoveRoom() etc. are not enough
        // This part might need refinement based on actual Change class capabilities
        // For now, we rely on std::get_if with the concrete types.
        // Change.h has isRemoveRoom(), let's check if it has isMakePermanent() or similar.
        // It does not. So std::get_if is the way.
        return std::nullopt;
    };

    std::optional<RoomId> optRoomId = getRoomIdFromLifecycleChange(change);

    if (optRoomId.has_value()) {
        RoomId roomId = optRoomId.value();
        PendingRoomOperation currentPendingOp = PendingRoomOperation::NONE;
        auto it = pendingRoomOperations.find(roomId);
        if (it != pendingRoomOperations.end()) {
            currentPendingOp = it->second;
        }

        if (std::holds_alternative<room_change_types::MakePermanent>(change.getVariant())) {
            if (currentPendingOp == PendingRoomOperation::NONE) {
                pendingRoomOperations[roomId] = PendingRoomOperation::MAKE_PERMANENT;
                changes.add(change); // Add to actual ChangeList
            } else if (currentPendingOp == PendingRoomOperation::MAKE_PERMANENT) {
                // Already pending make permanent, do nothing (redundant)
            } else if (currentPendingOp == PendingRoomOperation::REMOVE_ROOM) {
                // Already pending remove, remove takes precedence. Do nothing.
            }
        } else if (std::holds_alternative<room_change_types::RemoveRoom>(change.getVariant())) {
            if (currentPendingOp == PendingRoomOperation::NONE || currentPendingOp == PendingRoomOperation::MAKE_PERMANENT) {
                // If it was pending make_permanent, remove overrides it.
                // If it was none, it becomes remove.
                pendingRoomOperations[roomId] = PendingRoomOperation::REMOVE_ROOM;
                changes.add(change); // Add to actual ChangeList
            } else if (currentPendingOp == PendingRoomOperation::REMOVE_ROOM) {
                // Already pending remove, do nothing (redundant)
            }
        } else {
            // Should not happen if optRoomId was valued based on MakePermanent or RemoveRoom
            changes.add(change); // Default for safety, though logic error if here
        }
    } else {
        // Not a MakePermanent or RemoveRoom change, add directly
        changes.add(change);
    }
}

// Optional helper methods (implementations)
bool PathEventContext::isOperationPending(RoomId id, PendingRoomOperation op) const {
    auto it = pendingRoomOperations.find(id);
    if (it != pendingRoomOperations.end()) {
        return it->second == op;
    }
    return false;
}

void PathEventContext::recordOperation(RoomId id, PendingRoomOperation op) {
    pendingRoomOperations[id] = op;
}

PendingRoomOperation PathEventContext::getPendingOperation(RoomId id) const {
    auto it = pendingRoomOperations.find(id);
    if (it != pendingRoomOperations.end()) {
        return it->second;
    }
    return PendingRoomOperation::NONE;
}

} // namespace mmapper
