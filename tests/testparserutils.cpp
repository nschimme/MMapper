// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors (for test utility functions)

#include <QtTest/QtTest>
#include <QString>
#include <QChar>
#include <QDebug> // For QDebug operators
#include <vector>
#include <string>
#include <algorithm> // For std::reverse, std::abs
#include <cmath>     // For std::abs 

// Minimal necessary includes for the utility functions & tests
#include "../src/global/Consts.h"      // For char_consts
#include "../src/map/enums.h"          // For RoomTerrainEnum, RoomLightEnum, ExitDirEnum etc.
#include "../src/map/coordinate.h"     // For Coordinate
#include "../src/map/mmapper2room.h"   // For Mmapper2Exit, RoomName, RoomDesc, RoomContents, makeRoomDesc etc.
#include "../src/map/ExitDirection.h"  // For ::exitDir global function
#include "../src/global/TextUtils.h"   // For mmqt::* functions
#include "../src/global/parserutils.h" // For ParserUtils::removeAnsiMarksInPlace
#include "../src/global/logging.h"     // For MMLOG if used by dependencies
#include "../src/global/Charset.h"     // For mmqt::toStdStringUtf8

// Includes for createParseEventTest
#include "../src/map/parseevent.h"
#include "../src/map/RawRoom.h" // Includes RawExit.h, RoomArea, ServerExitIds etc.
#include "../src/map/PromptFlags.h"
#include "../src/map/ConnectedRoomFlags.h"
#include "../src/map/sanitizer.h" // For sanitizer functions used by sanitize helpers
#include "../src/map/CommandId.h"  // For CommandEnum used in ParseEvent

// Anonymous namespace for the utility functions and helpers to be tested
namespace {

// --- Sanitizer helpers (originally from testparser.cpp) ---
// These are no longer needed as calls are replaced with direct sanitizer calls.

// --- Utility functions copied from src/parser/abstractparser.cpp ---
// getTerrainSymbol was moved to ParserUtils, this local static version is no longer needed.
// getLightSymbol was moved to ParserUtils, this local static version is no longer needed.
// compressDirections was moved to ParserUtils, this local static version is no longer needed.

// --- QDebug operators (originally from testparser.cpp) ---
// These need to be available for QCOMPARE on custom types in createParseEventTest
static QDebug operator<<(QDebug debug, const ExitDirEnum dir)
{
#define X_CASE_EXITDIRENUM(UPPER) case ExitDirEnum::UPPER: return debug << "ExitDirEnum::" #UPPER
    switch (dir) {
        X_CASE_EXITDIRENUM(NORTH); X_CASE_EXITDIRENUM(SOUTH); X_CASE_EXITDIRENUM(EAST); X_CASE_EXITDIRENUM(WEST);
        X_CASE_EXITDIRENUM(UP); X_CASE_EXITDIRENUM(DOWN); X_CASE_EXITDIRENUM(UNKNOWN); X_CASE_EXITDIRENUM(NONE);
    default: return debug << "*error*";
    }
#undef X_CASE_EXITDIRENUM
}

static QDebug operator<<(QDebug debug, const ExitFlagEnum flag)
{
#define X_CASE_EXITFLAGENUM(_UPPER, _lower, _Camel, _Friendly) case ExitFlagEnum::_UPPER: return debug << "ExitFlagEnum::" #_UPPER;
    switch (flag) { XFOREACH_EXIT_FLAG(X_CASE_EXITFLAGENUM) }
    return debug << "*error*";
#undef X_CASE_EXITFLAGENUM
}

static QDebug operator<<(QDebug debug, const ExitFlags flags)
{
    auto &&ns = debug.nospace(); ns << "ExitFlags{"; auto prefix = "";
    for (auto f : flags) { ns << prefix; prefix = " | "; ns << f; }
    ns << "}"; return debug;
}

static QDebug operator<<(QDebug debug, const ExitsFlagsType f)
{
    auto &&ns = debug.nospace(); ns << "ExitsFlagsType{";
    if (f != ExitsFlagsType{}) {
        ns << ".valid=" << f.isValid();
        for (auto dir : ALL_EXITS_NESWUD) { // ALL_EXITS_NESWUD is from enums.h
            auto x = f.get(dir); if (!x.empty()) { ns << ", [" << dir << "] = " << x; } } }
    ns << "}"; return debug;
}


} // anonymous namespace

class TestParserUtils : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testCompressDirections();
    void testGetTerrainSymbol();
    void testGetLightSymbol();
    void removeAnsiMarksTest(); 
    void toAsciiTest();    
    void createParseEventTest(); // Moved from TestParser
};

void TestParserUtils::testCompressDirections() { /* ... as before ... */ 
    QCOMPARE(ParserUtils::compressDirections(""), QString(" (here)"));
    QCOMPARE(ParserUtils::compressDirections("n"), QString("n (total: 1n)"));
    QCOMPARE(ParserUtils::compressDirections("nnn"), QString("3n (total: 3n)"));
    QCOMPARE(ParserUtils::compressDirections("nnsse"), QString("2n2se (total: 1s 1e)")); 
    QCOMPARE(ParserUtils::compressDirections("ns"), QString("ns (total: )"));
    QCOMPARE(ParserUtils::compressDirections("ew"), QString("ew (total: )"));
    QCOMPARE(ParserUtils::compressDirections("ud"), QString("ud (total: )"));
    QCOMPARE(ParserUtils::compressDirections("2n1s"), QString("2n1s (total: 1n)"));
    QCOMPARE(ParserUtils::compressDirections("3e1w1n"), QString("3e1w1n (total: 1n 2e)"));
    QCOMPARE(ParserUtils::compressDirections("wwwsssuud"), QString("3w3s2ud (total: 1s 3w 1u)"));
    QCOMPARE(ParserUtils::compressDirections("neswud"), QString("neswud (total: )"));
    QCOMPARE(ParserUtils::compressDirections("2n2e2s2w"), QString("2n2e2s2w (total: )"));
    QCOMPARE(ParserUtils::compressDirections("3n1s1e2w1u2d"), QString("3n1s1e2w1u2d (total: 2n 1w 1d)"));
}

void TestParserUtils::testGetTerrainSymbol() { /* ... as before ... */ 
    using namespace char_consts;
    QCOMPARE(ParserUtils::getTerrainSymbol(RoomTerrainEnum::UNDEFINED), C_SPACE);
    QCOMPARE(ParserUtils::getTerrainSymbol(RoomTerrainEnum::INDOORS), C_OPEN_BRACKET);
    QCOMPARE(ParserUtils::getTerrainSymbol(RoomTerrainEnum::CITY), C_POUND_SIGN);
    QCOMPARE(ParserUtils::getTerrainSymbol(RoomTerrainEnum::FIELD), C_PERIOD);
    QCOMPARE(ParserUtils::getTerrainSymbol(RoomTerrainEnum::FOREST), 'f');
    QCOMPARE(ParserUtils::getTerrainSymbol(RoomTerrainEnum::HILLS), C_OPEN_PARENS);
    QCOMPARE(ParserUtils::getTerrainSymbol(RoomTerrainEnum::MOUNTAINS), C_LESS_THAN);
    QCOMPARE(ParserUtils::getTerrainSymbol(RoomTerrainEnum::SHALLOW), C_PERCENT_SIGN);
    QCOMPARE(ParserUtils::getTerrainSymbol(RoomTerrainEnum::WATER), C_TILDE);
    QCOMPARE(ParserUtils::getTerrainSymbol(RoomTerrainEnum::RAPIDS), 'W');
    QCOMPARE(ParserUtils::getTerrainSymbol(RoomTerrainEnum::UNDERWATER), 'U');
    QCOMPARE(ParserUtils::getTerrainSymbol(RoomTerrainEnum::ROAD), C_PLUS_SIGN);
    QCOMPARE(ParserUtils::getTerrainSymbol(RoomTerrainEnum::TUNNEL), C_EQUALS);
    QCOMPARE(ParserUtils::getTerrainSymbol(RoomTerrainEnum::CAVERN), 'O');
    QCOMPARE(ParserUtils::getTerrainSymbol(RoomTerrainEnum::BRUSH), C_COLON);
    QCOMPARE(ParserUtils::getTerrainSymbol(static_cast<RoomTerrainEnum>(99)), C_SPACE); 
}

void TestParserUtils::testGetLightSymbol() { /* ... as before ... */ 
    using namespace char_consts;
    QCOMPARE(ParserUtils::getLightSymbol(RoomLightEnum::DARK), 'o');
    QCOMPARE(ParserUtils::getLightSymbol(RoomLightEnum::LIT), C_ASTERISK);
    QCOMPARE(ParserUtils::getLightSymbol(RoomLightEnum::UNDEFINED), C_ASTERISK);
    QCOMPARE(ParserUtils::getLightSymbol(static_cast<RoomLightEnum>(99)), C_QUESTION_MARK);
}

void TestParserUtils::removeAnsiMarksTest() { /* ... as before ... */ 
    QString ansiString("\033[32mHello world\033[0m");
    QString expected("Hello world");
    ParserUtils::removeAnsiMarksInPlace(ansiString);
    QCOMPARE(ansiString, expected);
}

void TestParserUtils::toAsciiTest() { /* ... as before ... */ 
    const QString qs("N\u00F3rui N\u00EDnui");
    QCOMPARE(qs.length(), 11); {
        const QString expectedAscii("Norui Ninui");
        QCOMPARE(expectedAscii.length(), 11);
        auto copy = qs; mmqt::toAsciiInPlace(copy); 
        QCOMPARE(copy, expectedAscii);
    } {
        const auto latin1 = mmqt::toStdStringLatin1(qs); 
        QCOMPARE(latin1.length(), 11); QCOMPARE(latin1[1], '\xF3'); QCOMPARE(latin1[7], '\xED');
    } {
        const auto utf8 = mmqt::toStdStringUtf8(qs); 
        QCOMPARE(utf8.length(), 13); QCOMPARE(utf8[1], '\xC3'); QCOMPARE(utf8[2], '\xB3');
        QCOMPARE(utf8[8], '\xC3'); QCOMPARE(utf8[9], '\xAD');
    }
}

void TestParserUtils::createParseEventTest() // Copied from TestParser
{
    // Uses sanitize helpers now in the anonymous namespace of this file
    static constexpr auto terrain = RoomTerrainEnum::INDOORS;
    auto check = [](const RoomName &roomName,
                    const RoomDesc &parsedRoomDescription,
                    const PromptFlagsType pFlags,
                    const size_t expectSkipped) {
        RoomContents roomContents = mmqt::makeRoomContents("Contents");
        ConnectedRoomFlagsType cFlags;
        cFlags.setValid();
        const ParseEvent e = ParseEvent::createEvent(CommandEnum::NORTH, // From CommandId.h
                                                     INVALID_SERVER_ROOMID, // From roomid.h (via RawRoom.h)
                                                     RoomArea{}, // From mmapper2room.h
                                                     roomName,
                                                     parsedRoomDescription,
                                                     roomContents,
                                                     ServerExitIds{}, // From roomid.h (via RawRoom.h)
                                                     terrain,
                                                     RawExits{}, // From RawExit.h (via RawRoom.h)
                                                     pFlags, // From PromptFlags.h
                                                     cFlags); // From ConnectedRoomFlags.h

        if ((false)) { // Disabled qInfo from original
            qDebug() << e;
        }
        QCOMPARE(e.getRoomName(), RoomName{sanitizer::sanitizeOneLine(roomName.toStdStringUtf8())});
        QCOMPARE(e.getRoomDesc(), RoomDesc{sanitizer::sanitizeMultiline(parsedRoomDescription.toStdStringUtf8())});
        QCOMPARE(e.getRoomContents(), RoomContents{sanitizer::sanitizeMultiline(roomContents.toStdStringUtf8())});
        if (e.getExitsFlags() != ExitsFlagsType{}) { // ExitsFlagsType from ExitsFlags.h (via parseevent.h)
            qInfo() << e.getExitsFlags() << " vs " << ExitsFlagsType{};
        }
        QCOMPARE(e.getExitsFlags(), ExitsFlagsType{});
        QCOMPARE(e.getPromptFlags(), pFlags);
        QCOMPARE(e.getConnectedRoomFlags(), cFlags);

        QCOMPARE(e.getMoveType(), CommandEnum::NORTH);
        QCOMPARE(e.getNumSkipped(), expectSkipped);
    };

    const auto name = RoomName{"Room"};
    const auto desc = makeRoomDesc("Description"); // from mmapper2room.h
    const auto promptFlags = []() {
        PromptFlagsType pf{};
        pf.setValid();
        return pf;
    }();

    check(name, desc, promptFlags, 0);
    check(name, desc, {}, 1);
    check(name, {}, promptFlags, 1);
    check({}, desc, promptFlags, 1);
    check(name, {}, {}, 2);
    check({}, desc, {}, 2);
    check({}, {}, promptFlags, 2);
    check({}, {}, {}, 3);
}


QTEST_MAIN(TestParserUtils)
#include "testparserutils.moc"
