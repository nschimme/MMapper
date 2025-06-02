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
template<typename T>
T sanitize(const T &input) = delete;
template<>
RoomName sanitize<>(const RoomName &nameStr)
{
    return RoomName{sanitizer::sanitizeOneLine(nameStr.toStdStringUtf8())};
}
template<>
RoomDesc sanitize<>(const RoomDesc &descStr)
{
    return RoomDesc{sanitizer::sanitizeMultiline(descStr.toStdStringUtf8())};
}
template<>
RoomContents sanitize<>(const RoomContents &descStr)
{
    return RoomContents{sanitizer::sanitizeMultiline(descStr.toStdStringUtf8())};
}

// --- Utility functions copied from src/parser/abstractparser.cpp ---
NODISCARD static char getTerrainSymbol(const RoomTerrainEnum type)
{
    switch (type) {
    case RoomTerrainEnum::UNDEFINED:    return char_consts::C_SPACE;
    case RoomTerrainEnum::INDOORS:      return char_consts::C_OPEN_BRACKET;
    case RoomTerrainEnum::CITY:         return char_consts::C_POUND_SIGN;
    case RoomTerrainEnum::FIELD:        return char_consts::C_PERIOD;
    case RoomTerrainEnum::FOREST:       return 'f';
    case RoomTerrainEnum::HILLS:        return char_consts::C_OPEN_PARENS;
    case RoomTerrainEnum::MOUNTAINS:    return char_consts::C_LESS_THAN;
    case RoomTerrainEnum::SHALLOW:      return char_consts::C_PERCENT_SIGN;
    case RoomTerrainEnum::WATER:        return char_consts::C_TILDE;
    case RoomTerrainEnum::RAPIDS:       return 'W';
    case RoomTerrainEnum::UNDERWATER:   return 'U';
    case RoomTerrainEnum::ROAD:         return char_consts::C_PLUS_SIGN;
    case RoomTerrainEnum::TUNNEL:       return char_consts::C_EQUALS;
    case RoomTerrainEnum::CAVERN:       return 'O';
    case RoomTerrainEnum::BRUSH:        return char_consts::C_COLON;
    }
    return char_consts::C_SPACE;
}

NODISCARD static char getLightSymbol(const RoomLightEnum lightType)
{
    switch (lightType) {
    case RoomLightEnum::DARK: return 'o';
    case RoomLightEnum::LIT:
    case RoomLightEnum::UNDEFINED: return char_consts::C_ASTERISK;
    }
    return char_consts::C_QUESTION_MARK;
}

NODISCARD static QString compressDirections(const QString &original)
{
    QString ans; int curnum = 0; QChar curval = char_consts::C_NUL; Coordinate delta;
    const auto addDirs = [&ans, &curnum, &curval, &delta]() {
        Q_ASSERT(curnum >= 1); Q_ASSERT(curval != char_consts::C_NUL);
        if (curnum > 1) ans.append(QString::number(curnum));
        ans.append(curval);
        const auto dir = Mmapper2Exit::dirForChar(curval.toLatin1());
        delta += ::exitDir(dir) * curnum; 
    };
    for (const QChar c : original) {
        if (curnum != 0 && curval == c) { ++curnum; } 
        else { if (curnum != 0) { addDirs(); } curnum = 1; curval = c; }
    }
    if (curnum != 0) { addDirs(); }
    bool wantDelta = true;
    if (wantDelta) {
        auto addNumber = [&curnum, &curval, &addDirs, &ans](const int n, const char pos, const char neg) {
            if (n == 0) { return; }
            curnum = std::abs(n); curval = (n < 0) ? neg : pos;
            ans += char_consts::C_SPACE; addDirs(); 
        }; 
        if (delta.isNull()) { ans += " (here)"; } 
        else {
            ans += " (total:";
            addNumber(delta.x, 'e', 'w'); addNumber(delta.y, 'n', 's'); addNumber(delta.z, 'u', 'd'); 
            ans += ")";
        }
    }
    return ans;
}

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
    QCOMPARE(compressDirections(""), QString(" (here)"));
    QCOMPARE(compressDirections("n"), QString("n (total: 1n)"));
    QCOMPARE(compressDirections("nnn"), QString("3n (total: 3n)"));
    QCOMPARE(compressDirections("nnsse"), QString("2n2se (total: 1s 1e)")); 
    QCOMPARE(compressDirections("ns"), QString("ns (total: )"));
    QCOMPARE(compressDirections("ew"), QString("ew (total: )"));
    QCOMPARE(compressDirections("ud"), QString("ud (total: )"));
    QCOMPARE(compressDirections("2n1s"), QString("2n1s (total: 1n)"));
    QCOMPARE(compressDirections("3e1w1n"), QString("3e1w1n (total: 1n 2e)"));
    QCOMPARE(compressDirections("wwwsssuud"), QString("3w3s2ud (total: 1s 3w 1u)"));
    QCOMPARE(compressDirections("neswud"), QString("neswud (total: )"));
    QCOMPARE(compressDirections("2n2e2s2w"), QString("2n2e2s2w (total: )"));
    QCOMPARE(compressDirections("3n1s1e2w1u2d"), QString("3n1s1e2w1u2d (total: 2n 1w 1d)"));
}

void TestParserUtils::testGetTerrainSymbol() { /* ... as before ... */ 
    using namespace char_consts;
    QCOMPARE(getTerrainSymbol(RoomTerrainEnum::UNDEFINED), C_SPACE);
    QCOMPARE(getTerrainSymbol(RoomTerrainEnum::INDOORS), C_OPEN_BRACKET);
    QCOMPARE(getTerrainSymbol(RoomTerrainEnum::CITY), C_POUND_SIGN);
    QCOMPARE(getTerrainSymbol(RoomTerrainEnum::FIELD), C_PERIOD);
    QCOMPARE(getTerrainSymbol(RoomTerrainEnum::FOREST), 'f');
    QCOMPARE(getTerrainSymbol(RoomTerrainEnum::HILLS), C_OPEN_PARENS);
    QCOMPARE(getTerrainSymbol(RoomTerrainEnum::MOUNTAINS), C_LESS_THAN);
    QCOMPARE(getTerrainSymbol(RoomTerrainEnum::SHALLOW), C_PERCENT_SIGN);
    QCOMPARE(getTerrainSymbol(RoomTerrainEnum::WATER), C_TILDE);
    QCOMPARE(getTerrainSymbol(RoomTerrainEnum::RAPIDS), 'W');
    QCOMPARE(getTerrainSymbol(RoomTerrainEnum::UNDERWATER), 'U');
    QCOMPARE(getTerrainSymbol(RoomTerrainEnum::ROAD), C_PLUS_SIGN);
    QCOMPARE(getTerrainSymbol(RoomTerrainEnum::TUNNEL), C_EQUALS);
    QCOMPARE(getTerrainSymbol(RoomTerrainEnum::CAVERN), 'O');
    QCOMPARE(getTerrainSymbol(RoomTerrainEnum::BRUSH), C_COLON);
    QCOMPARE(getTerrainSymbol(static_cast<RoomTerrainEnum>(99)), C_SPACE); 
}

void TestParserUtils::testGetLightSymbol() { /* ... as before ... */ 
    using namespace char_consts;
    QCOMPARE(getLightSymbol(RoomLightEnum::DARK), 'o');
    QCOMPARE(getLightSymbol(RoomLightEnum::LIT), C_ASTERISK);
    QCOMPARE(getLightSymbol(RoomLightEnum::UNDEFINED), C_ASTERISK);
    QCOMPARE(getLightSymbol(static_cast<RoomLightEnum>(99)), C_QUESTION_MARK);
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
        QCOMPARE(e.getRoomName(), sanitize(roomName));
        QCOMPARE(e.getRoomDesc(), sanitize(parsedRoomDescription));
        QCOMPARE(e.getRoomContents(), sanitize(roomContents));
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
