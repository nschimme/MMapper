#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2021-2024 The MMapper Authors & Jeff Plaisance

#include "../global/BppOrderedSet.h" // Include the new generic BppOrderedSet
#include "roomid.h"                  // For RoomId and ExternalRoomId types

// The template class detail::BasicRoomIdSet<Type_> has been moved to ../global/BppOrderedSet.h
// and renamed to BppOrderedSet<Type_>.

// Type alias for RoomIdSet using the generic BppOrderedSet
using RoomIdSet = BppOrderedSet<RoomId>;

// Type alias for ExternalRoomIdSet using the generic BppOrderedSet
using ExternalRoomIdSet = BppOrderedSet<ExternalRoomId>;

namespace test {
// Declaration for the test function, implementation is in RoomIdSet.cpp
extern void testRoomIdSet();
} // namespace test
