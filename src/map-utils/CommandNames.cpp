// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "CommandNames.h"

#include <QByteArray>

#define CASE3(E, UPPER, s, n) \
    case E::UPPER: \
        return Abbrev{s, n};

Abbrev getParserCommandName(const DoorFlagEnum x)
{
    switch (x) {
        CASE3(DoorFlagEnum, HIDDEN, "hidden", 3)
        CASE3(DoorFlagEnum, NEED_KEY, "needkey", -1)
        CASE3(DoorFlagEnum, NO_BLOCK, "noblock", -1)
        CASE3(DoorFlagEnum, NO_BREAK, "nobreak", -1)
        CASE3(DoorFlagEnum, NO_PICK, "nopick", -1)
        CASE3(DoorFlagEnum, DELAYED, "delayed", 5)
        CASE3(DoorFlagEnum, CALLABLE, "callable", 4)
        CASE3(DoorFlagEnum, KNOCKABLE, "knockable", 6)
        CASE3(DoorFlagEnum, MAGIC, "magic", 3)
        CASE3(DoorFlagEnum, ACTION, "action", 3)
        CASE3(DoorFlagEnum, NO_BASH, "nobash", -1)
    }
    return Abbrev{};
}

Abbrev getParserCommandName(const RoomLightEnum x)
{
    switch (x) {
        CASE3(RoomLightEnum, UNDEFINED, "undefined", -1)
        CASE3(RoomLightEnum, LIT, "lit", -1)
        CASE3(RoomLightEnum, DARK, "dark", -1)
    }
    return Abbrev{};
}

Abbrev getParserCommandName(const RoomSundeathEnum x)
{
    switch (x) {
        CASE3(RoomSundeathEnum, UNDEFINED, "undefined", -1)
        CASE3(RoomSundeathEnum, NO_SUNDEATH, "nosundeath", -1)
        CASE3(RoomSundeathEnum, SUNDEATH, "sundeath", -1)
    }
    return Abbrev{};
}

Abbrev getParserCommandName(const RoomPortableEnum x)
{
    switch (x) {
        CASE3(RoomPortableEnum, UNDEFINED, "undefined", -1)
        CASE3(RoomPortableEnum, PORTABLE, "port", -1)
        CASE3(RoomPortableEnum, NOT_PORTABLE, "noport", -1)
    }
    return Abbrev{};
}

Abbrev getParserCommandName(const RoomRidableEnum x)
{
    switch (x) {
        CASE3(RoomRidableEnum, UNDEFINED, "undefined", -1)
        CASE3(RoomRidableEnum, RIDABLE, "ride", -1)
        CASE3(RoomRidableEnum, NOT_RIDABLE, "noride", -1)
    }
    return Abbrev{};
}

Abbrev getParserCommandName(const RoomAlignEnum x)
{
    switch (x) {
        CASE3(RoomAlignEnum, UNDEFINED, "undefined", -1)
        CASE3(RoomAlignEnum, GOOD, "good", -1)
        CASE3(RoomAlignEnum, NEUTRAL, "neutral", -1)
        CASE3(RoomAlignEnum, EVIL, "evil", -1)
    }
    return Abbrev{};
}

Abbrev getParserCommandName(const RoomMobFlagEnum x)
{
    switch (x) {
        CASE3(RoomMobFlagEnum, RENT, "rent", -1)
        CASE3(RoomMobFlagEnum, SHOP, "shop", -1)
        CASE3(RoomMobFlagEnum, WEAPON_SHOP, "weaponshop", -1)
        CASE3(RoomMobFlagEnum, ARMOUR_SHOP, "armourshop", -1)
        CASE3(RoomMobFlagEnum, FOOD_SHOP, "foodshop", -1)
        CASE3(RoomMobFlagEnum, PET_SHOP, "petshop", 3)
        CASE3(RoomMobFlagEnum, GUILD, "guild", -1)
        CASE3(RoomMobFlagEnum, SCOUT_GUILD, "scoutguild", 5)
        CASE3(RoomMobFlagEnum, MAGE_GUILD, "mageguild", 4)
        CASE3(RoomMobFlagEnum, CLERIC_GUILD, "clericguild", 6)
        CASE3(RoomMobFlagEnum, WARRIOR_GUILD, "warriorguild", 7)
        CASE3(RoomMobFlagEnum, RANGER_GUILD, "rangerguild", 6)
        CASE3(RoomMobFlagEnum, AGGRESSIVE_MOB, "aggmob", -1)
        CASE3(RoomMobFlagEnum, QUEST_MOB, "questmob", -1)
        CASE3(RoomMobFlagEnum, PASSIVE_MOB, "passivemob", -1)
        CASE3(RoomMobFlagEnum, ELITE_MOB, "elitemob", -1)
        CASE3(RoomMobFlagEnum, SUPER_MOB, "smob", -1)
        CASE3(RoomMobFlagEnum, MILKABLE, "milkable", -1)
        CASE3(RoomMobFlagEnum, RATTLESNAKE, "rattlesnake", -1)
    }
    return Abbrev{};
}

Abbrev getParserCommandName(const RoomLoadFlagEnum x)
{
    switch (x) {
        CASE3(RoomLoadFlagEnum, TREASURE, "treasure", -1)
        CASE3(RoomLoadFlagEnum, ARMOUR, "armour", -1)
        CASE3(RoomLoadFlagEnum, WEAPON, "weapon", -1)
        CASE3(RoomLoadFlagEnum, WATER, "water", -1)
        CASE3(RoomLoadFlagEnum, FOOD, "food", -1)
        CASE3(RoomLoadFlagEnum, HERB, "herb", -1)
        CASE3(RoomLoadFlagEnum, KEY, "key", -1)
        CASE3(RoomLoadFlagEnum, MULE, "mule", -1)
        CASE3(RoomLoadFlagEnum, HORSE, "horse", -1)
        CASE3(RoomLoadFlagEnum, PACK_HORSE, "pack", -1)
        CASE3(RoomLoadFlagEnum, TRAINED_HORSE, "trained", -1)
        CASE3(RoomLoadFlagEnum, ROHIRRIM, "rohirrim", -1)
        CASE3(RoomLoadFlagEnum, WARG, "warg", -1)
        CASE3(RoomLoadFlagEnum, BOAT, "boat", -1)
        CASE3(RoomLoadFlagEnum, ATTENTION, "attention", -1)
        CASE3(RoomLoadFlagEnum, TOWER, "watch", -1)
        CASE3(RoomLoadFlagEnum, CLOCK, "clock", -1)
        CASE3(RoomLoadFlagEnum, MAIL, "mail", -1)
        CASE3(RoomLoadFlagEnum, STABLE, "stable", -1)
        CASE3(RoomLoadFlagEnum, WHITE_WORD, "whiteword", -1)
        CASE3(RoomLoadFlagEnum, DARK_WORD, "darkword", -1)
        CASE3(RoomLoadFlagEnum, EQUIPMENT, "equipment", -1)
        CASE3(RoomLoadFlagEnum, COACH, "coach", -1)
        CASE3(RoomLoadFlagEnum, FERRY, "ferry", -1)
        CASE3(RoomLoadFlagEnum, DEATHTRAP, "deathtrap", -1)
    }
    return Abbrev{};
}

Abbrev getParserCommandName(const RoomTerrainEnum x)
{
    switch (x) {
        CASE3(RoomTerrainEnum, UNDEFINED, "undefined", -1)
        CASE3(RoomTerrainEnum, INDOORS, "indoors", -1)
        CASE3(RoomTerrainEnum, CITY, "city", -1)
        CASE3(RoomTerrainEnum, FIELD, "field", -1)
        CASE3(RoomTerrainEnum, FOREST, "forest", -1)
        CASE3(RoomTerrainEnum, HILLS, "hills", -1)
        CASE3(RoomTerrainEnum, MOUNTAINS, "mountains", -1)
        CASE3(RoomTerrainEnum, SHALLOW, "shallow", -1)
        CASE3(RoomTerrainEnum, WATER, "water", -1)
        CASE3(RoomTerrainEnum, RAPIDS, "rapids", -1)
        CASE3(RoomTerrainEnum, UNDERWATER, "underwater", -1)
        CASE3(RoomTerrainEnum, ROAD, "road", -1)
        CASE3(RoomTerrainEnum, BRUSH, "brush", -1)
        CASE3(RoomTerrainEnum, TUNNEL, "tunnel", -1)
        CASE3(RoomTerrainEnum, CAVERN, "cavern", -1)
    }
    return Abbrev{};
}

Abbrev getParserCommandName(const DoorActionEnum action)
{
    switch (action) {
        CASE3(DoorActionEnum, OPEN, "open", 2)
        CASE3(DoorActionEnum, CLOSE, "close", 3)
        CASE3(DoorActionEnum, LOCK, "lock", 3)
        CASE3(DoorActionEnum, UNLOCK, "unlock", 3)
        CASE3(DoorActionEnum, PICK, "pick", -1)
        CASE3(DoorActionEnum, ROCK, "rock", -1)
        CASE3(DoorActionEnum, BASH, "bash", -1)
        CASE3(DoorActionEnum, BREAK, "break", -1)
        CASE3(DoorActionEnum, BLOCK, "block", -1)
        CASE3(DoorActionEnum, KNOCK, "knock", -1)
    case DoorActionEnum::NONE:
        break;
    }
    return Abbrev{};
}

Abbrev getParserCommandName(const ExitFlagEnum x)
{
    switch (x) {
        CASE3(ExitFlagEnum, DOOR, "door", -1)
        CASE3(ExitFlagEnum, EXIT, "exit", -1)
        CASE3(ExitFlagEnum, ROAD, "road", -1)
        CASE3(ExitFlagEnum, CLIMB, "climb", 3)
        CASE3(ExitFlagEnum, RANDOM, "random", 4)
        CASE3(ExitFlagEnum, SPECIAL, "special", 4)
        CASE3(ExitFlagEnum, NO_MATCH, "nomatch", -1)
        CASE3(ExitFlagEnum, FLOW, "flow", -1)
        CASE3(ExitFlagEnum, NO_FLEE, "noflee", -1)
        CASE3(ExitFlagEnum, DAMAGE, "damage", -1)
        CASE3(ExitFlagEnum, FALL, "fall", -1)
        CASE3(ExitFlagEnum, GUARDED, "guarded", 5)
        CASE3(ExitFlagEnum, UNMAPPED, "unmapped", -1)
    }
    return Abbrev{};
}

Abbrev getParserCommandName(const InfomarkClassEnum x)
{
    switch (x) {
        CASE3(InfomarkClassEnum, GENERIC, "generic", -1)
        CASE3(InfomarkClassEnum, HERB, "herb", -1)
        CASE3(InfomarkClassEnum, RIVER, "river", 2)
        CASE3(InfomarkClassEnum, PLACE, "place", -1)
        CASE3(InfomarkClassEnum, MOB, "mob", -1)
        CASE3(InfomarkClassEnum, COMMENT, "comment", -1)
        CASE3(InfomarkClassEnum, ROAD, "road", 2)
        CASE3(InfomarkClassEnum, OBJECT, "object", -1)
        CASE3(InfomarkClassEnum, ACTION, "action", -1)
        CASE3(InfomarkClassEnum, LOCALITY, "locality", -1)
    }
    return Abbrev{};
}
