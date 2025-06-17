// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "RawRooms.h"

#include "../global/logging.h"

void RawRooms::setExitFlags_safe(const RoomId id, const ExitDirEnum dir, const ExitFlags flags)
{
    setExitExitFlags(id, dir, flags); // This now uses getMutable() via macro
    // The enforceInvariants call below will also need to use getMutable if it modifies.
    // Assuming ::enforceInvariants takes RawExit& and modifies it.
    enforceInvariants(id, dir);
}

void RawRooms::enforceInvariants(const RoomId id, const ExitDirEnum dir)
{
    // Pass a mutable RawExit to global ::enforceInvariants
    ::enforceInvariants(getRawRoomRef(id).getMutable()->getExit(dir));
}

void RawRooms::enforceInvariants(const RoomId id)
{
    // Pass a mutable RawRoom to global ::enforceInvariants
    ::enforceInvariants(*getRawRoomRef(id).getMutable());
}

bool RawRooms::satisfiesInvariants(const RoomId id, const ExitDirEnum dir) const
{
    // Pass a const RawExit to global ::satisfiesInvariants
    return ::satisfiesInvariants(getRawRoomRef(id).get()->getExit(dir));
}

bool RawRooms::satisfiesInvariants(const RoomId id) const
{
    // Pass a const RawRoom to global ::satisfiesInvariants
    return ::satisfiesInvariants(*getRawRoomRef(id).get());
}
