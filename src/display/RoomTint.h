#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2021 The MMapper Authors

#include "../global/Array.h"
#include <cstddef>

enum class NODISCARD RoomTintEnum { DARK, NO_SUNDEATH };
static const size_t NUM_ROOM_TINTS = 2;

NODISCARD extern const MMapper::Array<RoomTintEnum, NUM_ROOM_TINTS> &getAllRoomTints();
#define ALL_ROOM_TINTS getAllRoomTints()
