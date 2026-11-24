#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "../global/Flags.h"
#include "../map/DoorFlags.h"
#include "../map/ExitFlags.h"
#include "../map/infomark.h"
#include "../map/mmapper2room.h"
#include "Abbrev.h"
#include "DoorAction.h"

#include <string>
#include <vector>

#include <QByteArray>

extern const Abbrev cmdBack;
extern const Abbrev cmdDirections;
extern const Abbrev cmdDoorHelp;
extern const Abbrev cmdGroupTell;
extern const Abbrev cmdHelp;
extern const Abbrev cmdMap;
extern const Abbrev cmdSearch;
extern const Abbrev cmdSet;
extern const Abbrev cmdTime;
extern const Abbrev cmdTrollExit;
extern const Abbrev cmdVote;

// One entry per special command (the ones typed with the parser prefix, e.g. "_search").
// This table drives each command's own help ("_help <command>"), and lets anything that needs to
// list the commands (e.g. tool discovery) read their names and summaries without a parser
// instance. AbstractParser binds a handler to each entry when it registers the commands.
// TODO: the one-line overview in AbstractParser::parseHelp() still keeps its own list of
// descriptions; merging it into this table would change the user-visible text.
struct NODISCARD SpecialCommandInfo final
{
    Abbrev abbrev;
    std::string summary;      // one line, shown by "help <command>" unless detailedHelp is set
    std::string detailedHelp; // optional long help that replaces the one-line summary
};

NODISCARD const std::vector<SpecialCommandInfo> &getSpecialCommandInfos();

// Throws std::logic_error if the command is not in the table.
NODISCARD const SpecialCommandInfo &getSpecialCommandInfo(Abbrev abbrev);

NODISCARD QByteArray getCommandName(DoorActionEnum action);

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
