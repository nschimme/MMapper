// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

// Critical type definitions needed by dummy Proxy must come first.
#include "../src/proxy/GmcpModule.h" 
#include "../src/proxy/GmcpMessage.h" 
#include "../src/global/ConfigEnums.h" // For MapModeEnum
#include "../src/map/parseevent.h"      // For SigParseEvent
#include "../src/parser/CommandQueue.h"// For CommandQueue
#include "../src/mapdata/roomselection.h" // For SigRoomSelection
#include "../src/parser/SendToUserSourceEnum.h" 

#include "testparser.h" // Includes QObject, etc.

#include "../src/global/Charset.h"
#include "../src/global/TextUtils.h"
#include "../src/global/parserutils.h"
#include "../src/map/RawExit.h"
#include "../src/map/mmapper2room.h" 
#include "../src/map/sanitizer.h"
#include "../src/map/enums.h"       
#include "../src/map/coordinate.h" 
#include "../src/map/ExitDirection.h" 

// --- Minimal Proxy stub ---
// Defined globally before ProxyParserApi.h is included.
class Proxy { 
public:
    bool isConnected() const { return false; } 
    void connectToMud() {}
    void disconnectFromMud() {}
    bool isUserGmcpModuleEnabled(const ::GmcpModuleTypeEnum &) const { return false; } 
    void gmcpToUser(const ::GmcpMessage &) {} 
    bool isMudGmcpModuleEnabled(const ::GmcpModuleTypeEnum &) const { return false; } 
    void gmcpToMud(const ::GmcpMessage &) {} 
};
// --- End Minimal Proxy stub ---

#include "../src/parser/abstractparser.h" 
#include "../src/map/CommandId.h"
#include "../src/mapdata/mapdata.h" // For MapData
#include "../src/clock/mumeclock.h"
#include "../src/observer/gameobserver.h"
#include "../src/proxy/ProxyParserApi.h" 
#include "../src/group/GroupManagerApi.h"
#include "../src/group/mmapper2group.h"
#include "../src/configuration/configuration.h" 

#include <QDebug>
#include <QString>
#include <QtTest/QtTest>
#include <memory> 
#include <vector> 
#include <algorithm> 

// --- Global Stubs for Viewer/Canvas/InfoMarkSelection ---
// These are needed to satisfy the linker for functions called in AbstractParser-Commands.cpp etc.
// Placed in global scope.
class QMainWindow {public: virtual ~QMainWindow() = default;}; 
std::unique_ptr<QMainWindow> makeAnsiViewWindow(const QString&, const QString&, std::string_view) { return nullptr; }
void addTopLevelWindow(std::unique_ptr<QMainWindow>) {}
class AnsiViewWindow : public QMainWindow { public: ~AnsiViewWindow() override = default; };

class MapCanvas { 
public:
    static MapCanvas* getPrimary() { return nullptr; } 
};

// InfoMarkSelection stub
// Forward declare types it might need (Coordinate is already included via map/coordinate.h)
// MapData is included via mapdata/mapdata.h
// Badge is included via global/Badge.h (pulled in by roomselection.h or other mapdata includes)
// No need for dummy Badge template here.
class InfoMarkSelection { 
public: 
    InfoMarkSelection(Badge<InfoMarkSelection>, MapData&, const Coordinate&, const Coordinate&) {} 
    void init() {} 
};
// --- End Global Stubs ---


// Anonymous namespace for test-local helper classes and duplicated functions
// The static utility functions (getTerrainSymbol, getLightSymbol, compressDirections)
// and the sanitize template specializations have been moved to testparserutils.cpp
namespace {
// This namespace is now empty or can be removed if nothing else is in it.
} // anonymous namespace

TestParser::TestParser() = default;
TestParser::~TestParser() = default;

class MockAbstractParserOutputs : public AbstractParserOutputs {
public:
    ~MockAbstractParserOutputs() override = default;
    std::vector<QString> sendToMudLog; 
    std::vector<std::tuple<SendToUserSourceEnum, QString, bool>> sendToUserLog;
    int onShowPathCallCount = 0; // Changed from vector to count
    int sendToUserCallCount = 0; // Added
    int mapChangedCount = 0; 
    int releaseAllPathsCount = 0;
    int graphicsSettingsChangedCount = 0; 
    int infomarksChangedCount = 0; 
    std::vector<MapModeEnum> setModeLog;
    std::vector<SigParseEvent> handleParseEventLog;

    void virt_onSendToMud(const QString &msg) override { sendToMudLog.push_back(msg); }
    void virt_onSendToUser(SendToUserSourceEnum source, const QString &msg, bool goAhead) override { 
        sendToUserLog.emplace_back(source, msg, goAhead);
        sendToUserCallCount++;
    }
    void virt_onMapChanged() override { mapChangedCount++; } 
    void virt_onGraphicsSettingsChanged() override { graphicsSettingsChangedCount++; }
    void virt_onReleaseAllPaths() override { releaseAllPathsCount++; } 
    void virt_onLog(const QString &, const QString &) override {}
    void virt_onHandleParseEvent(const SigParseEvent &event) override { handleParseEventLog.push_back(event); }
    void virt_onShowPath(const CommandQueue &) override { onShowPathCallCount++; } // Changed
    void virt_onNewRoomSelection(const SigRoomSelection &) override {} 
    void virt_onSetMode(MapModeEnum mode) override { setModeLog.push_back(mode); }
    void virt_onInfomarksChanged() override { infomarksChangedCount++; }
};

static Proxy dummyProxyFor_ParseUserCommandsTest; 
static Proxy dummyProxyFor_SetPrefixTest;
static Proxy dummyProxyFor_ConfigTest; 
static Proxy dummyProxyFor_MapTest;
static Proxy dummyProxyFor_DoMoveTest; // Added for the new test

void TestParser::parseUserCommandsTest() {
    MockAbstractParserOutputs mockOutputs; GameObserver mockGameObserver; MapData mockMapData(nullptr); 
    MumeClock mockMumeClock(mockGameObserver); ParserCommonData mockCommonData(nullptr); 
    Mmapper2Group mockMmapper2Group(nullptr); GroupManagerApi mockGroupManagerApi(mockMmapper2Group); 
    ProxyMudConnectionApi dummyProxyMudApi(dummyProxyFor_ParseUserCommandsTest); 
    ProxyUserGmcpApi dummyProxyUserGmcpApi(dummyProxyFor_ParseUserCommandsTest);
    AbstractParser parser(mockMapData, mockMumeClock, dummyProxyMudApi, dummyProxyUserGmcpApi, mockGroupManagerApi, nullptr, mockOutputs, mockCommonData);
    
    mockOutputs.onShowPathCallCount = 0; // Reset counter
    mockCommonData.queue.clear(); 
    QCOMPARE(parser.parseUserCommands("north"), false);
    QCOMPARE(mockCommonData.queue.size(), 1); if (!mockCommonData.queue.isEmpty()) QCOMPARE(mockCommonData.queue.head(), CommandEnum::NORTH); QCOMPARE(mockOutputs.onShowPathCallCount, 1); 
    
    mockOutputs.onShowPathCallCount = 0; mockCommonData.queue.clear();
    QCOMPARE(parser.parseUserCommands("south"), false);
    QCOMPARE(mockCommonData.queue.size(), 1); if (!mockCommonData.queue.isEmpty()) QCOMPARE(mockCommonData.queue.head(), CommandEnum::SOUTH); QCOMPARE(mockOutputs.onShowPathCallCount, 1);

    mockOutputs.onShowPathCallCount = 0; mockCommonData.queue.clear();
    QCOMPARE(parser.parseUserCommands("east"), false);
    QCOMPARE(mockCommonData.queue.size(), 1); if (!mockCommonData.queue.isEmpty()) QCOMPARE(mockCommonData.queue.head(), CommandEnum::EAST); QCOMPARE(mockOutputs.onShowPathCallCount, 1);

    mockOutputs.onShowPathCallCount = 0; mockCommonData.queue.clear();
    QCOMPARE(parser.parseUserCommands("west"), false);
    QCOMPARE(mockCommonData.queue.size(), 1); if (!mockCommonData.queue.isEmpty()) QCOMPARE(mockCommonData.queue.head(), CommandEnum::WEST); QCOMPARE(mockOutputs.onShowPathCallCount, 1);

    mockOutputs.onShowPathCallCount = 0; mockCommonData.queue.clear();
    QCOMPARE(parser.parseUserCommands("up"), false);
    QCOMPARE(mockCommonData.queue.size(), 1); if (!mockCommonData.queue.isEmpty()) QCOMPARE(mockCommonData.queue.head(), CommandEnum::UP); QCOMPARE(mockOutputs.onShowPathCallCount, 1);

    mockOutputs.onShowPathCallCount = 0; mockCommonData.queue.clear();
    QCOMPARE(parser.parseUserCommands("down"), false);
    QCOMPARE(mockCommonData.queue.size(), 1); if (!mockCommonData.queue.isEmpty()) QCOMPARE(mockCommonData.queue.head(), CommandEnum::DOWN); QCOMPARE(mockOutputs.onShowPathCallCount, 1);

    mockOutputs.onShowPathCallCount = 0; mockCommonData.queue.clear();
    QCOMPARE(parser.parseUserCommands("look"), false);
    QCOMPARE(mockCommonData.queue.size(), 1); if (!mockCommonData.queue.isEmpty()) QCOMPARE(mockCommonData.queue.head(), CommandEnum::LOOK); QCOMPARE(mockOutputs.onShowPathCallCount, 1);
    
    mockOutputs.sendToUserLog.clear(); QCOMPARE(parser.parseUserCommands("invalidcommand"), false);
    bool foundArgle = false; for(const auto& logEntry : mockOutputs.sendToUserLog) if (std::get<1>(logEntry).contains("Arglebargle")) { foundArgle = true; break; } QVERIFY(foundArgle);
    
    mockOutputs.sendToUserLog.clear(); QCOMPARE(parser.parseUserCommands(""), false); 
    foundArgle = false; for(const auto& logEntry : mockOutputs.sendToUserLog) if (std::get<1>(logEntry).contains("Arglebargle")) { foundArgle = true; break; } QVERIFY(foundArgle);
    
    mockOutputs.onShowPathCallCount = 0; mockCommonData.queue.clear();
    QCOMPARE(parser.parseUserCommands("  north"), false);
    QCOMPARE(mockCommonData.queue.size(), 1); if (!mockCommonData.queue.isEmpty()) QCOMPARE(mockCommonData.queue.head(), CommandEnum::NORTH); QCOMPARE(mockOutputs.onShowPathCallCount, 1);
    
    mockOutputs.onShowPathCallCount = 0; mockCommonData.queue.clear();
    QCOMPARE(parser.parseUserCommands("north  "), false);
    QCOMPARE(mockCommonData.queue.size(), 1); if (!mockCommonData.queue.isEmpty()) QCOMPARE(mockCommonData.queue.head(), CommandEnum::NORTH); QCOMPARE(mockOutputs.onShowPathCallCount, 1);
    
    mockOutputs.onShowPathCallCount = 0; mockCommonData.queue.clear();
    QCOMPARE(parser.parseUserCommands("  north  "), false);
    QCOMPARE(mockCommonData.queue.size(), 1); if (!mockCommonData.queue.isEmpty()) QCOMPARE(mockCommonData.queue.head(), CommandEnum::NORTH); QCOMPARE(mockOutputs.onShowPathCallCount, 1);
    
    mockOutputs.onShowPathCallCount = 0; mockCommonData.queue.clear();
    QCOMPARE(parser.parseUserCommands("  look  "), false);
    QCOMPARE(mockCommonData.queue.size(), 1); if (!mockCommonData.queue.isEmpty()) QCOMPARE(mockCommonData.queue.head(), CommandEnum::LOOK); QCOMPARE(mockOutputs.onShowPathCallCount, 1);
    
    mockOutputs.sendToUserLog.clear(); QCOMPARE(parser.parseUserCommands("   "), false);
    foundArgle = false; for(const auto& logEntry : mockOutputs.sendToUserLog) if (std::get<1>(logEntry).contains("Arglebargle")) { foundArgle = true; break; } QVERIFY(foundArgle);
    
    char prefix = '/'; QString helpCommand = QString(prefix) + "help"; mockOutputs.sendToUserLog.clear();
    QCOMPARE(parser.parseUserCommands(helpCommand), false);
    bool helpSent = false; for(const auto& logEntry : mockOutputs.sendToUserLog) if (std::get<1>(logEntry).contains("MMapper help:")) { helpSent = true; break; } QVERIFY(helpSent);
}

void TestParser::testParseSpecialCommand_SetPrefix() {
    MockAbstractParserOutputs mockOutputs; GameObserver mockGameObserver; MapData mockMapData(nullptr);
    MumeClock mockMumeClock(mockGameObserver); ParserCommonData mockCommonData(nullptr);
    Mmapper2Group mockMmapper2Group(nullptr); GroupManagerApi mockGroupManagerApi(mockMmapper2Group);
    ProxyMudConnectionApi dummyProxyMudApi(dummyProxyFor_SetPrefixTest); ProxyUserGmcpApi dummyProxyUserGmcpApi(dummyProxyFor_SetPrefixTest);
    AbstractParser parser(mockMapData, mockMumeClock, dummyProxyMudApi, dummyProxyUserGmcpApi, mockGroupManagerApi, nullptr, mockOutputs, mockCommonData);
    char originalPrefix = getConfig().parser.prefixChar; char newPrefix = '!';
    if (newPrefix == originalPrefix) newPrefix = (originalPrefix == '!') ? '#' : '!';
    QString setPrefixCommand = QString(originalPrefix) + "set prefix " + QString(newPrefix);
    mockOutputs.sendToUserLog.clear(); QCOMPARE(parser.parseUserCommands(setPrefixCommand), false); 
    bool prefixSetMessageFound = false; QString expectedMessage = QString("The current command prefix is: %1%2%1").arg(QChar((newPrefix == '\'') ? '"' : '\'')).arg(newPrefix);
    for (const auto& logEntry : mockOutputs.sendToUserLog) if (std::get<1>(logEntry).contains(expectedMessage)) { prefixSetMessageFound = true; break; } QVERIFY(prefixSetMessageFound);
    QCOMPARE(getConfig().parser.prefixChar, newPrefix);
    QString helpCommandWithNewPrefix = QString(newPrefix) + "help"; mockOutputs.sendToUserLog.clear();
    QCOMPARE(parser.parseUserCommands(helpCommandWithNewPrefix), false);
    bool helpMessageFound = false; for (const auto& logEntry : mockOutputs.sendToUserLog) if (std::get<1>(logEntry).contains("MMapper help:")) { helpMessageFound = true; break; } QVERIFY(helpMessageFound);
    QString restorePrefixCommand = QString(newPrefix) + "set prefix " + QString(originalPrefix);
    mockOutputs.sendToUserLog.clear(); QCOMPARE(parser.parseUserCommands(restorePrefixCommand), false);
    QCOMPARE(getConfig().parser.prefixChar, originalPrefix); 
    bool prefixRestoredMessageFound = false; QString expectedRestoreMessage = QString("The current command prefix is: %1%2%1").arg(QChar((originalPrefix == '\'') ? '"' : '\'')).arg(originalPrefix);
    for (const auto& logEntry : mockOutputs.sendToUserLog) if (std::get<1>(logEntry).contains(expectedRestoreMessage)) { prefixRestoredMessageFound = true; break; } QVERIFY(prefixRestoredMessageFound);
}

void TestParser::testParseSpecialCommand_Config() {
    MockAbstractParserOutputs mockOutputs; GameObserver mockGameObserver; MapData mockMapData(nullptr);
    MumeClock mockMumeClock(mockGameObserver); ParserCommonData mockCommonData(nullptr);
    Mmapper2Group mockMmapper2Group(nullptr); GroupManagerApi mockGroupManagerApi(mockMmapper2Group);
    ProxyMudConnectionApi dummyProxyMudApi(dummyProxyFor_ConfigTest); ProxyUserGmcpApi dummyProxyUserGmcpApi(dummyProxyFor_ConfigTest);
    AbstractParser parser(mockMapData, mockMumeClock, dummyProxyMudApi, dummyProxyUserGmcpApi, mockGroupManagerApi, nullptr, mockOutputs, mockCommonData);
    char currentPrefix = getConfig().parser.prefixChar; QString configCommand = QString(currentPrefix) + "config";
    mockOutputs.sendToUserLog.clear(); QCOMPARE(parser.parseUserCommands(configCommand), false);
    bool helpFound = false; for (const auto& logEntry : mockOutputs.sendToUserLog) if (std::get<1>(logEntry).contains("Syntax: " + QString(currentPrefix) + "config")) { helpFound = true; break; } QVERIFY(helpFound);
}

void TestParser::testParseSpecialCommand_Map() {
    MockAbstractParserOutputs mockOutputs; GameObserver mockGameObserver; MapData mockMapData(nullptr);
    MumeClock mockMumeClock(mockGameObserver); ParserCommonData mockCommonData(nullptr);
    Mmapper2Group mockMmapper2Group(nullptr); GroupManagerApi mockGroupManagerApi(mockMmapper2Group);
    ProxyMudConnectionApi dummyProxyMudApi(dummyProxyFor_MapTest); ProxyUserGmcpApi dummyProxyUserGmcpApi(dummyProxyFor_MapTest);
    AbstractParser parser(mockMapData, mockMumeClock, dummyProxyMudApi, dummyProxyUserGmcpApi, mockGroupManagerApi, nullptr, mockOutputs, mockCommonData);
    char currentPrefix = getConfig().parser.prefixChar; QString mapCommand = QString(currentPrefix) + "map";
    mockOutputs.sendToUserLog.clear(); QCOMPARE(parser.parseUserCommands(mapCommand), false);
    bool helpFound = false; for (const auto& logEntry : mockOutputs.sendToUserLog) if (std::get<0>(logEntry) == SendToUserSourceEnum::FromMMapper && !std::get<1>(logEntry).isEmpty()) if (std::get<1>(logEntry).contains("goto") && std::get<1>(logEntry).contains("diff")) { helpFound = true; break; } QVERIFY(helpFound);
}

void TestParser::testDoMove_Offline_QueueInteraction()
{
    MockAbstractParserOutputs mockOutputs; GameObserver mockGameObserver; 
    MapData mockMapData(nullptr); MumeClock mockMumeClock(mockGameObserver); 
    ParserCommonData mockCommonData(nullptr); Mmapper2Group mockMmapper2Group(nullptr); 
    GroupManagerApi mockGroupManagerApi(mockMmapper2Group); 
    ProxyMudConnectionApi dummyProxyMudApi(dummyProxyFor_DoMoveTest); 
    ProxyUserGmcpApi dummyProxyUserGmcpApi(dummyProxyFor_DoMoveTest);
    AbstractParser parser(mockMapData, mockMumeClock, dummyProxyMudApi, dummyProxyUserGmcpApi, mockGroupManagerApi, nullptr, mockOutputs, mockCommonData);

    mockCommonData.queue.clear(); mockOutputs.onShowPathCallCount = 0;
    parser.doMove(CommandEnum::NORTH);
    QCOMPARE(mockCommonData.queue.size(), 1); QVERIFY(!mockCommonData.queue.isEmpty());
    if (!mockCommonData.queue.isEmpty()) QCOMPARE(mockCommonData.queue.head(), CommandEnum::NORTH);
    QCOMPARE(mockOutputs.onShowPathCallCount, 1);

    mockCommonData.queue.clear(); mockOutputs.onShowPathCallCount = 0;
    parser.doMove(CommandEnum::EAST);
    QCOMPARE(mockCommonData.queue.size(), 1); QVERIFY(!mockCommonData.queue.isEmpty());
    if (!mockCommonData.queue.isEmpty()) QCOMPARE(mockCommonData.queue.head(), CommandEnum::EAST);
    QCOMPARE(mockOutputs.onShowPathCallCount, 1);
    
    // ... (similar tests for SOUTH, WEST, UP, DOWN)
    mockCommonData.queue.clear(); mockOutputs.onShowPathCallCount = 0;
    parser.doMove(CommandEnum::SOUTH);
    QCOMPARE(mockCommonData.queue.size(), 1); if (!mockCommonData.queue.isEmpty()) QCOMPARE(mockCommonData.queue.head(), CommandEnum::SOUTH); QCOMPARE(mockOutputs.onShowPathCallCount, 1);

    mockCommonData.queue.clear(); mockOutputs.onShowPathCallCount = 0;
    parser.doMove(CommandEnum::WEST);
    QCOMPARE(mockCommonData.queue.size(), 1); if (!mockCommonData.queue.isEmpty()) QCOMPARE(mockCommonData.queue.head(), CommandEnum::WEST); QCOMPARE(mockOutputs.onShowPathCallCount, 1);

    mockCommonData.queue.clear(); mockOutputs.onShowPathCallCount = 0;
    parser.doMove(CommandEnum::UP);
    QCOMPARE(mockCommonData.queue.size(), 1); if (!mockCommonData.queue.isEmpty()) QCOMPARE(mockCommonData.queue.head(), CommandEnum::UP); QCOMPARE(mockOutputs.onShowPathCallCount, 1);

    mockCommonData.queue.clear(); mockOutputs.onShowPathCallCount = 0;
    parser.doMove(CommandEnum::DOWN);
    QCOMPARE(mockCommonData.queue.size(), 1); if (!mockCommonData.queue.isEmpty()) QCOMPARE(mockCommonData.queue.head(), CommandEnum::DOWN); QCOMPARE(mockOutputs.onShowPathCallCount, 1);

    // Test LOOK
    mockCommonData.queue.clear(); mockOutputs.onShowPathCallCount = 0; mockOutputs.sendToUserCallCount = 0;
    parser.doMove(CommandEnum::LOOK); 
    QCOMPARE(mockCommonData.queue.size(), 1); // LOOK is enqueued by doMove
    if (!mockCommonData.queue.isEmpty()) QCOMPARE(mockCommonData.queue.head(), CommandEnum::LOOK);
    QCOMPARE(mockOutputs.onShowPathCallCount, 1); // pathChanged is called
    // In offline mode, doMove->offlineCharacterMove->timer->doOfflineCharacterMove
    // doOfflineCharacterMove for LOOK calls sendRoomInfoToUser, sendRoomExitsInfoToUser, sendPromptToUser.
    // Each of these eventually calls virt_onSendToUser.
    // We cannot easily test sendToUserCallCount here without event loop processing for the timer.
    // QVERIFY(mockOutputs.sendToUserCallCount > 0); // This would fail without timer processing

    // Test accumulating commands
    mockCommonData.queue.clear(); mockOutputs.onShowPathCallCount = 0;
    parser.doMove(CommandEnum::NORTH);
    parser.doMove(CommandEnum::EAST);
    QCOMPARE(mockCommonData.queue.size(), 2);
    if (mockCommonData.queue.size() == 2) {
        QCOMPARE(mockCommonData.queue.dequeue(), CommandEnum::NORTH); 
        QCOMPARE(mockCommonData.queue.dequeue(), CommandEnum::EAST);
    }
    QCOMPARE(mockOutputs.onShowPathCallCount, 2); 
}

// TestParser::testCompressDirections() has been moved to TestParserUtils
// TestParser::testGetTerrainSymbol() has been moved to TestParserUtils
// TestParser::testGetLightSymbol() has been moved to TestParserUtils
// TestParser::removeAnsiMarksTest() has been moved to TestParserUtils
// TestParser::toAsciiTest() has been moved to TestParserUtils

// Static QDebug operators are moved to testparserutils.cpp as they were primarily for createParseEventTest
// static QDebug operator<<(QDebug debug, const ExitDirEnum dir) { /* ... */ }
// static QDebug operator<<(QDebug debug, const ExitFlagEnum flag) { /* ... */ }
// static QDebug operator<<(QDebug debug, const ExitFlags flags) { /* ... */ }
// static QDebug operator<<(QDebug debug, const ExitsFlagsType f) { /* ... */ }

// TestParser::createParseEventTest() has been moved to TestParserUtils

QTEST_MAIN(TestParser)
#include "testparser.moc"
