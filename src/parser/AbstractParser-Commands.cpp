// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "AbstractParser-Commands.h"

#include "../global/AsyncTasks.h"
#include "../global/CaseUtils.h"
#include "../global/Consts.h"
#include "../global/LineUtils.h"
#include "../global/StringView.h"
#include "../global/TextUtils.h"
#include "../global/progresscounter.h"
#include "../map/CommandId.h"
#include "../map/DoorFlags.h"
#include "../map/ExitFlags.h"
#include "../map/enums.h"
#include "../map/infomark.h"
#include "../mapdata/mapdata.h"
#include "../syntax/SyntaxArgs.h"
#include "../syntax/TreeParser.h"
#include "../viewers/AnsiViewWindow.h"
#include "../viewers/LaunchAsyncViewer.h"
#include "../viewers/TopLevelWindows.h"
#include "Abbrev.h"
#include "AbstractParser-Utils.h"
#include "DoorAction.h"
#include "abstractparser.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <QMessageLogContext>
#include <QtCore>

const Abbrev cmdBack{"back"};
const Abbrev cmdConfig{"config", 4};
const Abbrev cmdConnect{"connect", 4};
const Abbrev cmdDirections{"dirs", 3};
const Abbrev cmdDisconnect{"disconnect", 4};
const Abbrev cmdDoorHelp{"doorhelp", 5};
const Abbrev cmdGenerateBaseMap{"generate-base-map"};
const Abbrev cmdGroup{"group", 2};
const Abbrev cmdHelp{"help", 2};
const Abbrev cmdMap{"map"};
const Abbrev cmdMark{"mark", 3};
const Abbrev cmdRemoveDoorNames{"remove-secret-door-names"};
const Abbrev cmdRoom{"room", 2};
const Abbrev cmdSearch{"search", 3};
const Abbrev cmdSet{"set", 2};
const Abbrev cmdTime{"time", 2};
const Abbrev cmdTimer{"timer", 5};
const Abbrev cmdVote{"vote", 2};

Abbrev getParserCommandName(const DoorFlagEnum x)
{
#define CASE3(UPPER, s, n) \
    do { \
    case DoorFlagEnum::UPPER: \
        return Abbrev{s, n}; \
    } while (false)
    switch (x) {
        CASE3(HIDDEN, "hidden", 3);
        CASE3(NEED_KEY, "needkey", -1);
        CASE3(NO_BLOCK, "noblock", -1);
        CASE3(NO_BREAK, "nobreak", -1);
        CASE3(NO_PICK, "nopick", -1);
        CASE3(DELAYED, "delayed", 5);
        CASE3(CALLABLE, "callable", 4);
        CASE3(KNOCKABLE, "knockable", 6);
        CASE3(MAGIC, "magic", 3);
        CASE3(ACTION, "action", 3);
        CASE3(NO_BASH, "nobash", -1);
    }
    return Abbrev{};
#undef CASE3
}

Abbrev getParserCommandName(const RoomLightEnum x)
{
#define CASE3(UPPER, s, n) \
    do { \
    case RoomLightEnum::UPPER: \
        return Abbrev{s, n}; \
    } while (false)
    switch (x) {
        CASE3(UNDEFINED, "undefined", -1);
        CASE3(LIT, "lit", -1);
        CASE3(DARK, "dark", -1);
    }
    return Abbrev{};
#undef CASE3
}

Abbrev getParserCommandName(const RoomSundeathEnum x)
{
#define CASE3(UPPER, s, n) \
    do { \
    case RoomSundeathEnum::UPPER: \
        return Abbrev{s, n}; \
    } while (false)
    switch (x) {
        CASE3(UNDEFINED, "undefined", -1);
        CASE3(NO_SUNDEATH, "nosundeath", -1);
        CASE3(SUNDEATH, "sundeath", -1);
    }
    return Abbrev{};
#undef CASE3
}

Abbrev getParserCommandName(const RoomPortableEnum x)
{
#define CASE3(UPPER, s, n) \
    do { \
    case RoomPortableEnum::UPPER: \
        return Abbrev{s, n}; \
    } while (false)
    switch (x) {
        CASE3(UNDEFINED, "undefined", -1);
        CASE3(PORTABLE, "port", -1);
        CASE3(NOT_PORTABLE, "noport", -1);
    }
    return Abbrev{};
#undef CASE3
}

Abbrev getParserCommandName(const RoomRidableEnum x)
{
#define CASE3(UPPER, s, n) \
    do { \
    case RoomRidableEnum::UPPER: \
        return Abbrev{s, n}; \
    } while (false)
    switch (x) {
        CASE3(UNDEFINED, "undefined", -1);
        CASE3(RIDABLE, "ride", -1);
        CASE3(NOT_RIDABLE, "noride", -1);
    }
    return Abbrev{};
#undef CASE3
}

Abbrev getParserCommandName(const RoomAlignEnum x)
{
#define CASE3(UPPER, s, n) \
    do { \
    case RoomAlignEnum::UPPER: \
        return Abbrev{s, n}; \
    } while (false)
    switch (x) {
        CASE3(UNDEFINED, "undefined", -1);
        CASE3(GOOD, "good", -1);
        CASE3(NEUTRAL, "neutral", -1);
        CASE3(EVIL, "evil", -1);
    }
    return Abbrev{};
#undef CASE3
}

Abbrev getParserCommandName(const RoomMobFlagEnum x)
{
#define CASE3(UPPER, s, n) \
    do { \
    case RoomMobFlagEnum::UPPER: \
        return Abbrev{s, n}; \
    } while (false)
    switch (x) {
        CASE3(RENT, "rent", -1);
        CASE3(SHOP, "shop", -1);
        CASE3(WEAPON_SHOP, "weaponshop", -1); // conflict with "weapon"
        CASE3(ARMOUR_SHOP, "armourshop", -1); // conflict with "armour"
        CASE3(FOOD_SHOP, "foodshop", -1);     // conflict with "food"
        CASE3(PET_SHOP, "petshop", 3);
        CASE3(GUILD, "guild", -1);
        CASE3(SCOUT_GUILD, "scoutguild", 5);
        CASE3(MAGE_GUILD, "mageguild", 4);
        CASE3(CLERIC_GUILD, "clericguild", 6);
        CASE3(WARRIOR_GUILD, "warriorguild", 7);
        CASE3(RANGER_GUILD, "rangerguild", 6);
        CASE3(AGGRESSIVE_MOB, "aggmob", -1);
        CASE3(QUEST_MOB, "questmob", -1);
        CASE3(PASSIVE_MOB, "passivemob", -1);
        CASE3(ELITE_MOB, "elitemob", -1);
        CASE3(SUPER_MOB, "smob", -1);
        CASE3(MILKABLE, "milkable", -1);
        CASE3(RATTLESNAKE, "rattlesnake", -1);
    }
    return Abbrev{};
#undef CASE3
}

Abbrev getParserCommandName(const RoomLoadFlagEnum x)
{
#define CASE3(UPPER, s, n) \
    do { \
    case RoomLoadFlagEnum::UPPER: \
        return Abbrev{s, n}; \
    } while (false)
    switch (x) {
        CASE3(TREASURE, "treasure", -1);
        CASE3(ARMOUR, "armour", -1);
        CASE3(WEAPON, "weapon", -1);
        CASE3(WATER, "water", -1);
        CASE3(FOOD, "food", -1);
        CASE3(HERB, "herb", -1);
        CASE3(KEY, "key", -1);
        CASE3(MULE, "mule", -1);
        CASE3(HORSE, "horse", -1);
        CASE3(PACK_HORSE, "pack", -1);
        CASE3(TRAINED_HORSE, "trained", -1);
        CASE3(ROHIRRIM, "rohirrim", -1);
        CASE3(WARG, "warg", -1);
        CASE3(BOAT, "boat", -1);
        CASE3(ATTENTION, "attention", -1);
        CASE3(TOWER, "watch", -1);
        CASE3(CLOCK, "clock", -1);
        CASE3(MAIL, "mail", -1);
        CASE3(STABLE, "stable", -1);
        CASE3(WHITE_WORD, "whiteword", -1);
        CASE3(DARK_WORD, "darkword", -1);
        CASE3(EQUIPMENT, "equipment", -1);
        CASE3(COACH, "coach", -1);
        CASE3(FERRY, "ferry", -1);
        CASE3(DEATHTRAP, "deathtrap", -1);
    }
    return Abbrev{};
#undef CASE3
}

// NOTE: This isn't used by the parser (currently only used for filenames).
Abbrev getParserCommandName(const RoomTerrainEnum x)
{
#define CASE3(UPPER, s, n) \
    do { \
    case RoomTerrainEnum::UPPER: \
        return Abbrev{s, n}; \
    } while (false)
    switch (x) {
        CASE3(UNDEFINED, "undefined", -1);
        CASE3(INDOORS, "indoors", -1);
        CASE3(CITY, "city", -1);
        CASE3(FIELD, "field", -1);
        CASE3(FOREST, "forest", -1);
        CASE3(HILLS, "hills", -1);
        CASE3(MOUNTAINS, "mountains", -1);
        CASE3(SHALLOW, "shallow", -1);
        CASE3(WATER, "water", -1);
        CASE3(RAPIDS, "rapids", -1);
        CASE3(UNDERWATER, "underwater", -1);
        CASE3(ROAD, "road", -1);
        CASE3(BRUSH, "brush", -1);
        CASE3(TUNNEL, "tunnel", -1);
        CASE3(CAVERN, "cavern", -1);
    }
    return Abbrev{};
#undef CASE3
}

QByteArray getCommandName(const DoorActionEnum action)
{
#define CASE2(UPPER, s) \
    do { \
    case DoorActionEnum::UPPER: \
        return s; \
    } while (false)
    switch (action) {
        CASE2(OPEN, "open");
        CASE2(CLOSE, "close");
        CASE2(LOCK, "lock");
        CASE2(UNLOCK, "unlock");
        CASE2(PICK, "pick");
        CASE2(ROCK, "throw rock");
        CASE2(BASH, "bash");
        CASE2(BREAK, "cast 'break door'");
        CASE2(BLOCK, "cast 'block door'");
        CASE2(KNOCK, "knock");

    case DoorActionEnum::NONE:
        break;
    }

    // REVISIT: use "look" ?
    return "";
#undef CASE2
}

Abbrev getParserCommandName(const DoorActionEnum action)
{
#define CASE3(UPPER, s, n) \
    do { \
    case DoorActionEnum::UPPER: \
        return Abbrev{s, n}; \
    } while (false)
    switch (action) {
        CASE3(OPEN, "open", 2);
        CASE3(CLOSE, "close", 3);
        CASE3(LOCK, "lock", 3);
        CASE3(UNLOCK, "unlock", 3);
        CASE3(PICK, "pick", -1);
        CASE3(ROCK, "rock", -1);
        CASE3(BASH, "bash", -1);
        CASE3(BREAK, "break", -1);
        CASE3(BLOCK, "block", -1);
        CASE3(KNOCK, "knock", -1);
    case DoorActionEnum::NONE:
        break;
    }

    return Abbrev{};
#undef CASE3
}

Abbrev getParserCommandName(const ExitFlagEnum x)
{
#define CASE3(UPPER, s, n) \
    do { \
    case ExitFlagEnum::UPPER: \
        return Abbrev{s, n}; \
    } while (false)
    switch (x) {
        CASE3(DOOR, "door", -1);
        CASE3(EXIT, "exit", -1);
        CASE3(ROAD, "road", -1);
        CASE3(CLIMB, "climb", 3);
        CASE3(RANDOM, "random", 4);
        CASE3(SPECIAL, "special", 4);
        CASE3(NO_MATCH, "nomatch", -1);
        CASE3(FLOW, "flow", -1);
        CASE3(NO_FLEE, "noflee", -1);
        CASE3(DAMAGE, "damage", -1);
        CASE3(FALL, "fall", -1);
        CASE3(GUARDED, "guarded", 5);
        CASE3(UNMAPPED, "unmapped", -1);
    }
    return Abbrev{};
#undef CASE3
}

Abbrev getParserCommandName(const InfomarkClassEnum x)
{
#define CASE3(UPPER, s, n) \
    do { \
    case InfomarkClassEnum::UPPER: \
        return Abbrev{s, n}; \
    } while (false)
    switch (x) {
        CASE3(GENERIC, "generic", -1);
        CASE3(HERB, "herb", -1);
        CASE3(RIVER, "river", 2);
        CASE3(PLACE, "place", -1);
        CASE3(MOB, "mob", -1);
        CASE3(COMMENT, "comment", -1);
        CASE3(ROAD, "road", 2);
        CASE3(OBJECT, "object", -1);
        CASE3(ACTION, "action", -1);
        CASE3(LOCALITY, "locality", -1);
    }
    return Abbrev{};
#undef CASE3
}

NODISCARD static bool isCommand(const std::string &str, Abbrev abbrev)
{
    if (!abbrev) {
        return false;
    }

    auto view = StringView{str}.trim();
    if (view.isEmpty()) {
        return false;
    }

    const auto word = view.takeFirstWord();
    return abbrev.matches(word);
}

NODISCARD static bool isCommand(const std::string &str, const CommandEnum cmd)
{
    switch (cmd) {
    case CommandEnum::NORTH:
    case CommandEnum::SOUTH:
    case CommandEnum::EAST:
    case CommandEnum::WEST:
    case CommandEnum::UP:
    case CommandEnum::DOWN:
    case CommandEnum::FLEE:
        // REVISIT: Add support for 'charge' and 'escape' commands
        return isCommand(str, Abbrev{getLowercase(cmd), 1});

    case CommandEnum::SCOUT:
        return isCommand(str, Abbrev{getLowercase(cmd), 2});

    case CommandEnum::LOOK:
        return isCommand(str, Abbrev{getLowercase(cmd), 1}) || isCommand(str, Abbrev{"examine", 3});

    case CommandEnum::UNKNOWN:
    case CommandEnum::NONE:
        return false;
    }

    return false;
}

bool AbstractParser::parseUserCommands(const QString &input)
{
    if (input.startsWith(getPrefixChar())) {
        std::string s = mmqt::toStdStringUtf8(input);
        auto view = StringView{s}.trim();
        if (view.isEmpty() || view.takeFirstLetter() != getPrefixChar()) {
            sendToUser(SendToUserSourceEnum::FromMMapper, "Internal error. Sorry.\n");
        } else {
            parseSpecialCommand(view);
        }
        sendPromptToUser();
        return false;
    }

    return parseSimpleCommand(input);
}

bool AbstractParser::parseSimpleCommand(const QString &qstr)
{
    const std::string str = mmqt::toStdStringUtf8(qstr);
    const auto isOnline = ::isOnline();

    for (const CommandEnum cmd : ALL_COMMANDS) {
        if (cmd == CommandEnum::NONE || cmd == CommandEnum::UNKNOWN) {
            continue;
        }

        if (!isCommand(str, cmd)) {
            continue;
        }

        switch (cmd) {
        case CommandEnum::NORTH:
        case CommandEnum::SOUTH:
        case CommandEnum::EAST:
        case CommandEnum::WEST:
        case CommandEnum::UP:
        case CommandEnum::DOWN:
            doMove(cmd);
            return isOnline;

        case CommandEnum::LOOK: {
            // if 'look' has arguments then it isn't valid for prespam
            auto view = StringView{str}.trim();
            if (view.countWords() == 1) {
                doMove(cmd);
                return isOnline;
            }
        } break;

        case CommandEnum::FLEE:
            if (!isOnline) {
                offlineCharacterMove(CommandEnum::FLEE);
                return false; // do not send command to mud server for offline mode
            }
            break;

        case CommandEnum::SCOUT:
            if (!isOnline) {
                auto view = StringView{str}.trim();
                if (!view.isEmpty() && !view.takeFirstWord().isEmpty()) {
                    const auto dir = static_cast<CommandEnum>(tryGetDir(view));
                    if (!isDirectionNESWUD(dir)) {
                        sendToUser(SendToUserSourceEnum::SimulatedOutput,
                                   "In which direction do you want to scout?\n");
                        sendPromptToUser();

                    } else {
                        auto &queue = getQueue();
                        queue.enqueue(CommandEnum::SCOUT);
                        queue.enqueue(dir);
                        offlineCharacterMove();
                    }
                    return false;
                }
            }
            break;

        case CommandEnum::UNKNOWN:
        case CommandEnum::NONE:
            break;
        }

        break; /* break the for loop */
    }

    if (!isOnline) {
        sendToUser(SendToUserSourceEnum::SimulatedOutput, "Arglebargle, glop-glyf!?!\n");
        sendPromptToUser();
    }

    return isOnline; // only forward command to mud server if online
}

bool AbstractParser::parseDoorAction(const DoorActionEnum dat, StringView words)
{
    const auto dir = tryGetDir(words);
    if (!words.isEmpty()) {
        return false;
    }
    performDoorCommand(dir, dat);
    return true;
}

void AbstractParser::parseSetCommand(StringView view)
{
    if (view.isEmpty()) {
        sendToUser(SendToUserSourceEnum::FromMMapper,
                   QString("Syntax: %1set prefix [punct-char]\n").arg(getPrefixChar()));
        return;
    }

    auto first = view.takeFirstWord();
    if (Abbrev{"prefix", 3}.matches(first)) {
        if (view.isEmpty()) {
            showCommandPrefix();
            return;
        }

        auto next = view.takeFirstWord();
        if (next.length() == 3) {
            auto quote = next.takeFirstLetter();
            const bool validQuote = quote == char_consts::C_SQUOTE
                                    || quote == char_consts::C_DQUOTE;
            const auto prefix = next.takeFirstLetter();

            if (validQuote && isValidPrefix(prefix) && quote == next.takeFirstLetter()
                && quote != prefix && setCommandPrefix(prefix)) {
                return;
            }
        } else if (next.length() == 1) {
            const auto prefix = next.takeFirstLetter();
            if (setCommandPrefix(prefix)) {
                return;
            }
        }

        sendToUser(SendToUserSourceEnum::FromMMapper, "Invalid prefix.\n");
        return;
    }

    sendToUser(SendToUserSourceEnum::FromMMapper, "That variable is not supported.");
}

void AbstractParser::parseSpecialCommand(StringView wholeCommand)
{
    if (wholeCommand.isEmpty()) {
        sendToUser(SendToUserSourceEnum::FromMMapper, "Error: special command input is empty.\n");
        return;
    }

    if (evalSpecialCommandMap(wholeCommand)) {
        return;
    }

    const auto word = wholeCommand.takeFirstWord();
    sendToUser(SendToUserSourceEnum::FromMMapper,
               QString("Unrecognized command: %1\n").arg(word.toQString()));
}

void AbstractParser::parseSearch(StringView view)
{
    if (view.isEmpty()) {
        showSyntax(
            "search [-regex] [-(name|desc|contents|note|exits|flags|area|all|clear)] pattern");
    } else {
        doSearchCommand(view);
    }
}

void AbstractParser::parseDirections(StringView view)
{
    if (view.isEmpty()) {
        showSyntax("dirs [-(name|desc|contents|note|exits|flags|all)] pattern");
    } else {
        doGetDirectionsCommand(view);
    }
}

class NODISCARD ArgHelpCommand final : public syntax::IArgument
{
private:
    const AbstractParser::ParserRecordMap &m_map;

public:
    explicit ArgHelpCommand(const AbstractParser::ParserRecordMap &map)
        : m_map(map)
    {}

private:
    NODISCARD syntax::MatchResult virt_match(const syntax::ParserInput &input,
                                             syntax::IMatchErrorLogger * /*logger*/) const final;

    std::ostream &virt_to_stream(std::ostream &os) const final;
};

syntax::MatchResult ArgHelpCommand::virt_match(const syntax::ParserInput &input,
                                               syntax::IMatchErrorLogger *) const
{
    auto &arg = *this;
    if (!input.empty()) {
        const auto &map = arg.m_map;
        const auto &next = input.front();
        const auto it = map.find(next);
        if (it != map.end()) {
            return syntax::MatchResult::success(1, input, Value(next));
        }
    }

    return syntax::MatchResult::failure(input);
}

std::ostream &ArgHelpCommand::virt_to_stream(std::ostream &os) const
{
    return os << "<command>";
}

void AbstractParser::parseHelp(StringView words)
{
    using namespace syntax;
    static const auto abb = syntax::abbrevToken;
    auto simpleSyntax = [](const std::string_view name, auto &&fn) -> SharedConstSublist {
        auto abbrev = abb(std::string(name));
        auto helpstr = std::string("help for ") + std::string(name);
        return buildSyntax(std::move(abbrev), Accept(fn, std::move(helpstr)));
    };

    auto syntax = buildSyntax(
        /* Note: The next section has optional "topic" to avoid collision. */
        buildSyntax(TokenMatcher::alloc<ArgHelpCommand>(m_specialCommandMap),
                    Accept(
                        [this](User &user, const Pair *const matched) -> void {
                            auto &os = user.getOstream();
                            if (!matched || !matched->car.isString()) {
                                os << "Internal error.\n";
                                return;
                            }
                            const auto &map = m_specialCommandMap;
                            const auto &name = matched->car.getString();
                            const auto it = map.find(name);
                            if (it == map.end()) {
                                os << "Internal error.\n";
                                return;
                            }
                            it->second.help(name);
                        },
                        "detailed help pages")),

        /* optional "topic" allows you to [/help topic door] to see the door help topic;
         * alternately, "[/help doors] should work too. */
        buildSyntax(TokenMatcher::alloc<ArgOptionalToken>(abb("topic")),
                    simpleSyntax("abbreviations",
                                 [this](auto &&, auto &&) { showHelpCommands(true); }),
                    simpleSyntax("commands", [this](auto &&, auto &&) { showHelpCommands(false); }),
                    simpleSyntax("doors", [this](auto &&, auto &&) { showDoorCommandHelp(); }),
                    simpleSyntax("miscellaneous", [this](auto &&, auto &&) { showMiscHelp(); })),
        Accept([this](auto &&, auto &&) { showHelp(); }, "general help"));

    eval("help", syntax, words);
}

void AbstractParser::doMapDiff()
{
    auto &mapData = m_mapData;

    if (mapData.getSavedMap().getInfomarkDb() != mapData.getCurrentMap().getInfomarkDb()) {
        sendToUser(SendToUserSourceEnum::FromMMapper,
                   "Note: Map markers have changed, but marker diff is not yet supported.\n");
    }

    const Map &origin = mapData.getSavedMap();
    const Map &map = mapData.getCurrentMap();

    if (map == origin) {
        sendToUser(SendToUserSourceEnum::FromMMapper,
                   "The map has not been modified since the last save.\n");
        return;
    }

    struct NODISCARD MapPair final
    {
        Map origin;
        Map current;
    };
    launchAsyncAnsiViewerWorker<MapPair>("map show diff",
                                         "Map Diff",
                                         {origin, map},
                                         [](ProgressCounter &pc, AnsiOstream &aos, MapPair &pair) {
                                             Map::diff(pc, aos, pair.origin, pair.current);
                                         });
}

void AbstractParser::doMapCommand(StringView input)
{
    auto compact = [this]() {
        auto &map = m_mapData;
        if (map.applySingleChange(Change{world_change_types::CompactRoomIds{}})) {
            // Mark *everything* as changed.
            map.setSavedMap(Map{});
            sendOkToUser();
            sendToUser(SendToUserSourceEnum::FromMMapper,
                       "WARNING: You should save the map immediately.\n");

        } else {
            sendToUser(SendToUserSourceEnum::FromMMapper, "Ooops.\n");
        }
    };
    auto removeDoorNames = [this]() { this->doRemoveDoorNamesCommand(); };
    auto generateBaseMap = [this]() {
        //std::ostream &os = std::cout;
        this->doGenerateBaseMap();
    };

    auto diff = [this]() { doMapDiff(); };

    auto printMulti = [this]() {
        // TODO: multi should probably select the rooms in question.
        launchAsyncAnsiViewerWorker<Map>("map show multi",
                                         "Exits with Multiple Connections",
                                         m_mapData.getCurrentMap(),
                                         [](ProgressCounter &pc, AnsiOstream &aos, Map &map) {
                                             map.printMulti(pc, aos);
                                         });
    };
    auto printStats = [this]() {
        launchAsyncAnsiViewerWorker<Map>("map show stats",
                                         "Map Stats",
                                         m_mapData.getCurrentMap(),
                                         [](ProgressCounter &pc, AnsiOstream &aos, Map &map) {
                                             map.printStats(pc, aos);
                                         });
    };

    auto printUnknown = [this]() {
        // TODO: unknown should probably select the rooms in question.
        launchAsyncAnsiViewerWorker<Map>("map show unknowns",
                                         "Rooms with Unknown Connections",
                                         m_mapData.getCurrentMap(),
                                         [](ProgressCounter &pc, AnsiOstream &aos, Map &map) {
                                             map.printUnknown(pc, aos);
                                         });
    };

    auto checkConsistency = [this]() {
        sendToUser(SendToUserSourceEnum::FromMMapper, "Attempting to check map consistency...\n");
        emit m_mapData.sig_checkMapConsistency();
    };

    auto revert = [this]() {
        MapData &map = m_mapData;

        try {
            map.revert();
        } catch (const std::exception &ex) {
            sendToUser(SendToUserSourceEnum::FromMMapper, std::string{"Exception: "} + ex.what());
            return;
        }

        sendOkToUser();
    };

    auto syn = [](std::string name, std::string help, std::function<void()> input_callback) {
        auto token = syntax::stringToken(std::move(name));
        auto fn = [callback = std::move(input_callback)](User & /*user*/,
                                                         const Pair *const /*args*/) { callback(); };
        return syntax::buildSyntax(token, syntax::Accept(fn, std::move(help)));
    };

    auto destructiveSyntax
        = syntax::buildSyntax(syntax::stringToken("destructive"),
                              syn("remove-hidden-door-names",
                                  "removes hidden door names",
                                  removeDoorNames),
                              syn("generate-base-map", "generate the base map", generateBaseMap),
                              syn("compact-ids", "compact the map IDs", compact),
                              syn("really-revert", "revert to the saved map", revert));

    auto diffSyntax = syn("diff", "show changes since the last save", diff);
    auto statsSynax = syn("stats", "print some statistics", printStats);

    auto showSyntax = syntax::buildSyntax(syntax::stringToken("show"),
                                          syn("multi",
                                              "show non-random exits with multiple connections",
                                              printMulti),
                                          syn("unknowns",
                                              "show rooms with legacy UNKNOWN exit directions",
                                              printUnknown));

    auto consistSyntax = syn("check-consistency", "checks map consistency", checkConsistency);

    auto gotoFn = [this](User &user, const Pair *const args) {
        auto &os = user.getOstream();
        const auto v = getAnyVectorReversed(args);

        if constexpr (IS_DEBUG_BUILD) {
            const auto &gotoString = v[0].getString();
            assert(gotoString == "goto");
        }

        const RoomId id = getOtherRoom(v[1].getInt());

        auto &mapData = m_mapData;
        const auto other = mapData.findRoomHandle(id);
        if (!other) {
            os << "To what RoomId?\n";
            return;
        }

        mapData.forceToRoom(id);
        doMove(CommandEnum::LOOK);
        send_ok(os);
    };

    auto gotoSyntax = syntax::buildSyntax(syntax::abbrevToken("goto"),
                                          syntax::TokenMatcher::alloc<syntax::ArgInt>(),
                                          syntax::Accept(gotoFn, "go to room #"));

    auto undeleteFn = [this](User &user, const Pair *const args) {
        auto &os = user.getOstream();
        const auto v = getAnyVectorReversed(args);

        if constexpr (IS_DEBUG_BUILD) {
            const auto &gotoString = v[0].getString();
            assert(gotoString == "undelete");
        }

        auto &mapData = m_mapData;
        const auto &saved = mapData.getSavedMap();
        const auto &current = mapData.getCurrentMap();

        // compare to RoomId AbstractParser::getOtherRoom() const
        // Here we're getting a room from the saved map, instead of from the current map.
        const int otherRoomId = v[1].getInt();
        if (otherRoomId < 0) {
            os << "RoomId cannot be negative.\n";
            return;
        }
        const auto otherExt = ExternalRoomId{static_cast<uint32_t>(otherRoomId)};

        if (current.findRoomHandle(otherExt)) {
            os << "That room is not deleted.\n";
            return;
        }

        const RoomHandle other = saved.findRoomHandle(otherExt);
        if (!other) {
            os << "That room does not exist in the saved copy of the map.\n";
            return;
        }

        auto rawCopy = other.getRaw();
        for (auto &e : rawCopy.exits) {
            e.incoming = {};
            e.outgoing = {};
        }

        if (!mapData.applySingleChange(Change{room_change_types::UndeleteRoom{otherExt, rawCopy}})) {
            os << "Failed to undelete the room.\n";
            return;
        }

        os << "Successfully undeleted room " << otherExt.asUint32() << ".\n";
    };

    auto undeleteSyntax = syntax::buildSyntax(syntax::abbrevToken("undelete"),
                                              syntax::TokenMatcher::alloc<syntax::ArgInt>(),
                                              syntax::Accept(undeleteFn,
                                                             "undelete room # (if possible)"));

    auto mapSyntax = syntax::buildSyntax(gotoSyntax,
                                         diffSyntax,
                                         statsSynax,
                                         showSyntax,
                                         destructiveSyntax,
                                         undeleteSyntax,
                                         consistSyntax);

    eval("map", mapSyntax, input);
}

void AbstractParser::initSpecialCommandMap()
{
    auto &map = m_specialCommandMap;
    map.clear();

    auto add = [this](Abbrev abb, const ParserCallback &callback, const HelpCallback &help) {
        addSpecialCommand(abb.getCommand(), abb.getMinAbbrev(), callback, help);
    };

    const auto makeSimpleHelp = [this](const std::string &help) {
        return [this, help](const std::string &name) {
            sendToUser(SendToUserSourceEnum::FromMMapper,
                       QString("Help for %1%2:\n"
                               "  %3\n"
                               "\n")
                           .arg(getPrefixChar())
                           .arg(mmqt::toQStringUtf8(name))
                           .arg(mmqt::toQStringUtf8(help)));
        };
    };

    qInfo() << "Adding special commands to the map...";

    // help is important, so it comes first

    add(
        cmdHelp,
        [this](const std::vector<StringView> & /*s*/, StringView rest) {
            this->parseHelp(rest);
            return true;
        },
        // TODO: create a parse tree, and show all of the help topics.
        makeSimpleHelp("Provides help."));
    add(
        cmdDoorHelp,
        [this](const std::vector<StringView> & /*s*/, StringView rest) {
            if (!rest.isEmpty()) {
                return false;
            }
            this->showDoorCommandHelp();
            return true;
        },
        makeSimpleHelp("Help for door console commands."));

    // door actions
    for (const DoorActionEnum x : ALL_DOOR_ACTION_TYPES) {
        if (auto cmd = getParserCommandName(x)) {
            add(
                cmd,
                [this, x](const std::vector<StringView> & /*s*/, StringView rest) {
                    return parseDoorAction(x, rest);
                },
                makeSimpleHelp("Sets door action: " + std::string{cmd.getCommand()}));
        }
    }

    // misc commands

    add(
        cmdBack,
        [this](const std::vector<StringView> & /*s*/, StringView rest) {
            if (!rest.isEmpty()) {
                return false;
            }
            this->doBackCommand();
            return true;
        },
        makeSimpleHelp("Delete prespammed commands from queue."));
    add(
        cmdConfig,
        [this](const std::vector<StringView> & /*s*/, StringView rest) {
            this->doConfig(rest);
            return true;
        },
        makeSimpleHelp("Configuration commands."));
    add(
        cmdConnect,
        [this](const std::vector<StringView> & /*s*/, StringView /*rest*/) {
            this->doConnectToHost();
            return true;
        },
        makeSimpleHelp("Connect to the MUD."));
    add(
        cmdDirections,
        [this](const std::vector<StringView> & /*s*/, StringView rest) {
            this->parseDirections(rest);
            return true;
        },
        makeSimpleHelp("Prints directions to matching rooms."));
    add(
        cmdDisconnect,
        [this](const std::vector<StringView> & /*s*/, StringView /*rest*/) {
            this->doDisconnectFromHost();
            return true;
        },
        makeSimpleHelp("Disconnect from the MUD."));
    add(
        cmdRemoveDoorNames,
        [this](const std::vector<StringView> & /*s*/, StringView rest) {
            if (!rest.isEmpty()) {
                return false;
            }
            this->doRemoveDoorNamesCommand();
            return true;
        },
        makeSimpleHelp("Remove hidden door names."));
    add(
        cmdGenerateBaseMap,
        [this](const std::vector<StringView> & /*s*/, StringView rest) {
            if (!rest.isEmpty()) {
                return false;
            }
            this->doGenerateBaseMap();
            return true;
        },
        makeSimpleHelp("Generate the base map."));
    add(
        cmdMap,
        [this](const std::vector<StringView> & /*s*/, StringView rest) {
            doMapCommand(rest);
            return true;
        },
        makeSimpleHelp("Print the changes changes since the last save"));
    add(
        cmdSearch,
        [this](const std::vector<StringView> & /*s*/, StringView rest) {
            this->parseSearch(rest);
            return true;
        },
        makeSimpleHelp("Highlight matching rooms on the map."));
    add(
        cmdSet,
        [this](const std::vector<StringView> & /*s*/, StringView rest) {
            this->parseSetCommand(rest);
            return true;
        },
        [this](const std::string &name) {
            const char help[]
                = "Subcommands:\n"
                  "\tprefix              # Displays the current prefix.\n"
                  "\tprefix <punct-char> # Changes the current prefix.\n"
                  "\n"
                  // REVISIT: Does it actually support LATIN-1 punctuation like the degree symbol?
                  "Note: <punct-char> may be any ASCII punctuation character,\n"
                  "      which can be optionally single- or double-quoted.\n"
                  "\n"
                  "Examples to set prefix:\n"
                  "\tprefix /   # slash character\n"
                  "\tprefix '/' # single-quoted slash character\n"
                  "\tprefix \"/\" # double-quoted slash character\n"
                  "\tprefix '   # bare single-quote character\n"
                  "\tprefix \"'\" # double-quoted single-quote character\n"
                  "\tprefix \"   # bare double-quote character\n"
                  "\tprefix '\"' # single-quoted double-quote character\n"
                  "\n"
                  "Note: Quoted versions do not allow escape codes,\n"
                  "so you cannot do ''', '\\'', \"\"\", or \"\\\"\".";

            sendToUser(SendToUserSourceEnum::FromMMapper,
                       QString("Help for %1%2:\n"
                               "%3\n"
                               "\n")
                           .arg(getPrefixChar())
                           .arg(mmqt::toQStringUtf8(name))
                           .arg(mmqt::toQStringUtf8(help)));
        });
    add(
        cmdTime,
        [this](const std::vector<StringView> & /*s*/, StringView rest) {
            if (!rest.isEmpty()) {
                return false;
            }
            this->showMumeTime();
            return true;
        },
        makeSimpleHelp("Displays the current MUME time."));
    add(
        cmdVote,
        [this](const std::vector<StringView> & /*s*/, StringView rest) {
            if (!rest.isEmpty()) {
                return false;
            }
            this->openVoteURL();
            return true;
        },
        makeSimpleHelp("Launches a web browser so you can vote for MUME on TMC!"));

    /* mark commands */
    add(
        cmdMark,
        [this](const std::vector<StringView> & /*s*/, StringView rest) {
            parseMark(rest);
            return true;
        },
        makeSimpleHelp("Perform actions on the current marks."));

    /* room commands */
    add(
        cmdRoom,
        [this](const std::vector<StringView> & /*s*/, StringView rest) {
            parseRoom(rest);
            return true;
        },
        makeSimpleHelp("View or modify properties of the current room."));

    /* group commands */
    add(
        cmdGroup,
        [this](const std::vector<StringView> & /*s*/, StringView rest) {
            parseGroup(rest);
            return true;
        },
        makeSimpleHelp("Perform actions on the group manager."));

    /* timers command */
    add(
        cmdTimer,
        [this](const std::vector<StringView> & /*s*/, StringView rest) {
            parseTimer(rest);
            return true;
        },
        makeSimpleHelp("Add or remove simple timers and countdown timers."));

    qInfo() << "Total commands + abbreviations: " << map.size();
}

void AbstractParser::addSpecialCommand(const char *const s,
                                       const int minLen,
                                       const ParserCallback &callback,
                                       const HelpCallback &help)
{
    const auto abb = Abbrev{s, minLen};
    if (!abb) {
        throw std::invalid_argument("s");
    }

    const auto len = abb.getLength();
    const auto min = std::max(1, abb.getMinAbbrev());
    const std::string fullName = abb.getCommand();
    std::string key = fullName;
    auto &map = m_specialCommandMap;
    for (int i = len; i >= min; --i) {
        assert(i >= 0);
        key.resize(static_cast<unsigned int>(i));
        auto it = map.find(key);
        if (it == map.end()) {
            map.emplace(key, ParserRecord{fullName, callback, help});
        } else {
            qWarning() << ("unable to add " + mmqt::toQStringUtf8(key) + " for " + abb.describe());
        }
    }
}

bool AbstractParser::evalSpecialCommandMap(StringView args)
{
    if (args.empty()) {
        return false;
    }

    auto first = args.takeFirstWord();
    auto &map = m_specialCommandMap;

    const std::string key = toLowerUtf8(first.getStdStringView());
    auto it = map.find(key);
    if (it == map.end()) {
        return false;
    }

    // REVISIT: add # of calls to the record?
    ParserRecord &rec = it->second;
    const auto &s = rec.fullCommand;
    const auto matched = std::vector<StringView>{StringView{s}};
    return rec.callback(matched, args);
}
