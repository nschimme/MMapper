#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2021-2024 The MMapper Authors & Jeff Plaisance

#include "BasicSetWrapper.h" // The new template wrapper
#include "roomid.h"          // For RoomId and ExternalRoomId types

// RoomIdSet is now an alias for BasicSetWrapper specialized with RoomId.
// BasicSetWrapper provides methods like insertAll, first, last, containsElementNotIn,
// and forwards core operations to an underlying BppOrderedSet.
using RoomIdSet = BasicSetWrapper<RoomId>;

// ExternalRoomIdSet is an alias for BasicSetWrapper specialized with ExternalRoomId.
using ExternalRoomIdSet = BasicSetWrapper<ExternalRoomId>;

namespace test {
// Declaration for the test function, implementation is in RoomIdSet.cpp.
// This will test BasicSetWrapper<RoomId> via the RoomIdSet alias.
extern void testRoomIdSet();
} // namespace test
