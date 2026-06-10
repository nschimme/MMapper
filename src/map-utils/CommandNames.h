#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "../global/Abbrev.h"
#include "../map/DoorFlags.h"
#include "../map/ExitFlags.h"
#include "../map/enums.h"
#include "../map/infomark.h"
#include "../parser/DoorAction.h"

NODISCARD Abbrev getParserCommandName(DoorActionEnum action);
NODISCARD Abbrev getParserCommandName(DoorFlagEnum x);
NODISCARD Abbrev getParserCommandName(ExitFlagEnum x);
NODISCARD Abbrev getParserCommandName(InfomarkClassEnum x);
NODISCARD Abbrev getParserCommandName(RoomAlignEnum x);
NODISCARD Abbrev getParserCommandName(RoomLightEnum x);
NODISCARD Abbrev getParserCommandName(RoomLoadFlagEnum x);
NODISCARD Abbrev getParserCommandName(RoomMobFlagEnum x);
NODISCARD Abbrev getParserCommandName(RoomPortableEnum x);
NODISCARD Abbrev getParserCommandName(RoomRidableEnum x);
NODISCARD Abbrev getParserCommandName(RoomSundeathEnum x);
NODISCARD Abbrev getParserCommandName(RoomTerrainEnum x);
