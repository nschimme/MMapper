// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "RawRooms.h"

#include "../global/logging.h"

// Global enforceInvariants functions (assumed to exist from original code)
// These modify their arguments directly.
void enforceInvariants(RawExit &exit);
void enforceInvariants(RawRoom &room);
// Global satisfiesInvariants functions (assumed to exist from original code)
// These are const.
bool satisfiesInvariants(const RawExit &exit);
bool satisfiesInvariants(const RawRoom &room);


void RawRooms::setExitFlags_safe(const RoomId id, const ExitDirEnum dir, const ExitFlags flags)
{
    // Step 1: Set the flags. The macro setExitExitFlags handles COW internally.
    // This means m_rooms is updated after this call.
    setExitExitFlags(id, dir, flags);

    // Step 2: Get a fresh copy of the room (which now has the flags updated).
    // to_idx helper and bounds checking are assumed from RawRooms.h modifications.
    if (to_idx(id) >= m_rooms.size()) {
        throw InvalidMapOperation(); // Or handle error appropriately
    }
    RawRoom room_copy = getRawRoomRef(id); // Gets a const ref, but we copy it.

    // Step 3: Call enforceInvariantsOnCopy (which calls global ::enforceInvariants) on this copy.
    // This modifies room_copy.
    enforceInvariantsOnCopy(room_copy, id, dir);

    // Step 4: Set the modified copy back into the persistent vector.
    m_rooms = m_rooms.set(to_idx(id), room_copy);
}

// Implementation for the new enforceInvariantsOnCopy methods declared in RawRooms.h
void RawRooms::enforceInvariantsOnCopy(RawRoom& mutable_room_copy, RoomId /*id*/, ExitDirEnum dir) const
{
    // Calls the global ::enforceInvariants, which modifies mutable_room_copy.getExit(dir)
    ::enforceInvariants(mutable_room_copy.getExit(dir));
}

void RawRooms::enforceInvariantsOnCopy(RawRoom& mutable_room_copy, RoomId /*id*/) const
{
    // Calls the global ::enforceInvariants, which modifies mutable_room_copy
    ::enforceInvariants(mutable_room_copy);
}

// Const methods remain largely the same, as getRawRoomRef(id) provides const access.
bool RawRooms::satisfiesInvariants(const RoomId id, const ExitDirEnum dir) const
{
    return ::satisfiesInvariants(getRawRoomRef(id).getExit(dir));
}

bool RawRooms::satisfiesInvariants(const RoomId id) const
{
    return ::satisfiesInvariants(getRawRoomRef(id));
}
