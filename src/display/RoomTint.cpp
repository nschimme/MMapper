// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2021 The MMapper Authors

#include "RoomTint.h"

NODISCARD const MMapper::Array<RoomTintEnum, NUM_ROOM_TINTS> &getListOfRoomTints()
{
    static const MMapper::Array<RoomTintEnum, NUM_ROOM_TINTS> tints{RoomTintEnum::DARK,
                                                                   RoomTintEnum::NO_SUNDEATH};
    return tints;
}
