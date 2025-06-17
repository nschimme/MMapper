#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "../global/CopyOnWrite.h"
#include "RoomIdSet.h" // For std::set<RoomId>
#include <set> // Ensure std::set is available

// Define CowRoomIdSet
using CowRoomIdSet = CopyOnWrite<std::set<RoomId>>;
