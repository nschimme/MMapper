#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "../global/Array.h"
#include "../global/macros.h"

#include <cstddef>

enum class NODISCARD RoomTintEnum { DARK, NO_SUNDEATH };
static constexpr size_t NUM_ROOM_TINTS = 2;
NODISCARD const MMapper::Array<RoomTintEnum, NUM_ROOM_TINTS> &getAllRoomTints();
#define ALL_ROOM_TINTS getAllRoomTints()
