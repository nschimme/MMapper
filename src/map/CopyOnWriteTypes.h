#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "../global/CopyOnWrite.h"
#include "RoomIdSet.h" // For std::set<RoomId>
#include "RawRoom.h"   // For RawRoom definition
#include <set>        // Ensure std::set is available for CowRoomIdSet

// Define CowRoomIdSet
using CowRoomIdSet = CopyOnWrite<std::set<RoomId>>;

// Define CowRawRoom
using CowRawRoom = CopyOnWrite<RawRoom>;
