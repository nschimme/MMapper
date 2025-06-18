// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "RawRooms.h"

#include "../global/logging.h"

// The public enforceInvariants methods should also use the updateRoom pattern
// if they are intended to modify the state. If they are only for validation (const),
// their name is misleading. Assuming they intend to modify and "fix" invariants:
void RawRooms::enforceInvariants(const RoomId id, const ExitDirEnum dir)
{
    updateRoom(id, [dir](RawRoom room) {
        ::enforceInvariants(room.getExit(dir));
        return room;
    });
}

void RawRooms::enforceInvariants(const RoomId id)
{
    updateRoom(id, [](RawRoom room) {
        ::enforceInvariants(room);
        return room;
    });
}

bool RawRooms::satisfiesInvariants(const RoomId id, const ExitDirEnum dir) const
{
    return ::satisfiesInvariants(getRawRoomRef(id).getExit(dir));
}

bool RawRooms::satisfiesInvariants(const RoomId id) const
{
    return ::satisfiesInvariants(getRawRoomRef(id));
}
