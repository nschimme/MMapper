// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "TestProxy.h"

#include "../src/global/TextUtils.h"
#include "../src/observer/KnownRooms.h"
#include "../src/observer/KnownRoomsStore.h"
#include "../src/observer/KnownRoomsTracker.h"
#include "../src/proxy/GmcpMessage.h"
#include "../src/proxy/GmcpModule.h"
#include "../src/proxy/GmcpUtils.h"
#include "../src/proxy/telnetfilter.h"

#include <cstdint>
#include <optional>
#include <vector>

#include <QDataStream>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest/QtTest>

void TestProxy::escapeTest()
{
    QCOMPARE(GmcpUtils::escapeGmcpStringData(R"(12345)"), QString(R"(12345)"));
    QCOMPARE(GmcpUtils::escapeGmcpStringData(R"(1.0)"), QString(R"(1.0)"));
    QCOMPARE(GmcpUtils::escapeGmcpStringData(R"(true)"), QString(R"(true)"));
    QCOMPARE(GmcpUtils::escapeGmcpStringData(R"("Hello")"), QString(R"(\"Hello\")"));
    QCOMPARE(GmcpUtils::escapeGmcpStringData("\\\n\r\b\f\t"), QString(R"(\\\n\r\b\f\t)"));
}

void TestProxy::gmcpMessageDeserializeTest()
{
    GmcpMessage gmcp1 = GmcpMessage::fromRawBytes(R"(Core.Hello { "Hello": "world" })");
    QCOMPARE(gmcp1.getName().toQByteArray(), QByteArray("Core.Hello"));
    QCOMPARE(gmcp1.getJson()->toQString(), mmqt::toQStringUtf8(R"({ "Hello": "world" })"));

    GmcpMessage gmcp2 = GmcpMessage::fromRawBytes(R"(Core.Goodbye)");
    QCOMPARE(gmcp2.getName().toQByteArray(), QByteArray("Core.Goodbye"));
    QVERIFY(!gmcp2.getJson());

    GmcpMessage gmcp3 = GmcpMessage::fromRawBytes(R"(External.Discord.Hello)");
    QCOMPARE(gmcp3.getName().toQByteArray(), QByteArray("External.Discord.Hello"));
    QVERIFY(!gmcp3.getJson());
}

void TestProxy::gmcpMessageSerializeTest()
{
    GmcpMessage gmcp1(GmcpMessageTypeEnum::CORE_HELLO);
    QCOMPARE(gmcp1.toRawBytes(), QByteArray("Core.Hello"));

    GmcpMessage gmcp2(GmcpMessageTypeEnum::CORE_HELLO, GmcpJson{"{}"});
    QCOMPARE(gmcp2.toRawBytes(), QByteArray("Core.Hello {}"));
}

void TestProxy::gmcpModuleTest()
{
    GmcpModule module1("Char 1");
    QCOMPARE(mmqt::toQByteArrayUtf8(module1.getNormalizedName()), QByteArray("char"));
    QCOMPARE(module1.getVersion().asUint32(), 1u);
    QVERIFY(module1.isSupported());

    GmcpModule module2("Char.Skills 1");
    QCOMPARE(mmqt::toQByteArrayUtf8(module2.getNormalizedName()), QByteArray("char.skills"));
    QCOMPARE(module2.getVersion().asUint32(), 1u);
    QVERIFY(!module2.isSupported());

    GmcpModule module3("Room");
    QCOMPARE(mmqt::toQByteArrayUtf8(module3.getNormalizedName()), QByteArray("room"));
    QCOMPARE(module3.getVersion().asUint32(), 0u);
    QVERIFY(module3.isSupported());
}

void TestProxy::telnetFilterTest()
{
    test::test_telnetfilter();
}

void TestProxy::knownRoomsHistoryTest()
{
    KnownRooms known;
    QVERIFY(!known.setEnabled(true));
    QVERIFY(known.receive(GmcpMessage::fromRawBytes(R"(Char.Name {"name":"Bilbo"})")));
    // Fail closed from the start: nothing is known until the character's history says so.
    QVERIFY(!known.getSnapshot().isKnown(ServerRoomId{99}));
    QVERIFY(!known.getSnapshot().listComplete);
    QVERIFY(!known.receive(GmcpMessage::fromRawBytes("Room.Known.List [1,3,4711]")));
    // Chunks are trusted for additions as they arrive, before the list terminates.
    QVERIFY(known.getSnapshot().isKnown(ServerRoomId{4711}));
    QVERIFY(!known.getSnapshot().listComplete);
    QVERIFY(!known.receive(GmcpMessage::fromRawBytes("Room.Known.Add 42")));
    QVERIFY(known.getSnapshot().isKnown(ServerRoomId{42}));
    QVERIFY(!known.receive(GmcpMessage::fromRawBytes("Room.Known.List [19,3]")));
    QVERIFY(!known.getSnapshot().listComplete);
    QVERIFY(!known.receive(GmcpMessage::fromRawBytes("Room.Known.List null")));
    const auto snapshot = known.getSnapshot();
    QVERIFY(snapshot.listComplete);
    QCOMPARE(snapshot.rooms.size(), size_t{5});
    QVERIFY(snapshot.isKnown(ServerRoomId{4711}));
    QVERIFY(snapshot.isKnown(ServerRoomId{42}));
    QVERIFY(!snapshot.isKnown(INVALID_SERVER_ROOMID));
    QVERIFY(!snapshot.isKnown(ServerRoomId{99}));
    QVERIFY(!known.receive(GmcpMessage::fromRawBytes("Room.Known.Add 99")));
    QVERIFY(known.getSnapshot().isKnown(ServerRoomId{99}));
    QCOMPARE(known.getSnapshot().generation, snapshot.generation);
    QVERIFY(!snapshot.isKnown(ServerRoomId{99})); // immutable worker snapshot
    const auto revision = known.getSnapshot().revision;
    QVERIFY(!known.receive(GmcpMessage::fromRawBytes("Room.Known.Add 99")));
    QCOMPARE(known.getSnapshot().revision, revision);
    known.reset();
    QVERIFY(known.getSnapshot().generation != snapshot.generation);
    QVERIFY(!known.getSnapshot().listComplete);
    QVERIFY(known.getSnapshot().rooms.empty());
}

void TestProxy::knownRoomsSwitchTest()
{
    KnownRooms known;
    QVERIFY(!known.setEnabled(true));
    QVERIFY(known.receive(GmcpMessage::fromRawBytes(R"(Char.Name {"name":"Bilbo"})")));
    QVERIFY(!known.receive(GmcpMessage::fromRawBytes("Room.Known.List [1,2]")));
    QVERIFY(!known.receive(GmcpMessage::fromRawBytes(R"(Char.Name {"name":"Frodo"})")));
    QVERIFY(!known.receive(GmcpMessage::fromRawBytes("Room.Known.List [3,4]")));
    QVERIFY(known.receive(GmcpMessage::fromRawBytes("Room.Known.List null")));
    QVERIFY(!known.getSnapshot().listComplete);
    QVERIFY(!known.receive(GmcpMessage::fromRawBytes("Room.Known.List [7]")));
    QVERIFY(!known.receive(GmcpMessage::fromRawBytes("Room.Known.List null")));
    QCOMPARE(known.getSnapshot().rooms.size(), size_t{1});
    QVERIFY(!known.getSnapshot().isKnown(ServerRoomId{1}));
    QVERIFY(known.getSnapshot().isKnown(ServerRoomId{7}));
    QVERIFY(!known.receive(GmcpMessage::fromRawBytes("Core.Goodbye")));
    QVERIFY(!known.getSnapshot().listComplete);
    for (const auto *identity : {"Char.Name null", "Char.Name {}", "Char.Name"}) {
        QVERIFY(known.receive(GmcpMessage::fromRawBytes(R"(Char.Name {"name":"Bilbo"})")));
        QVERIFY(!known.receive(GmcpMessage::fromRawBytes("Room.Known.List [1]")));
        QVERIFY(!known.receive(GmcpMessage::fromRawBytes(identity)));
        QVERIFY(!known.receive(GmcpMessage::fromRawBytes("Room.Known.List null")));
        QVERIFY(!known.getSnapshot().listComplete);
    }
}

void TestProxy::knownRoomsUnavailableTest()
{
    KnownRooms known;
    QVERIFY(!known.receive(GmcpMessage::fromRawBytes(R"(Char.Name {"name":"Bilbo"})")));
    QVERIFY(known.setEnabled(true)); // can be enabled during play
    QVERIFY(!known.receive(GmcpMessage::fromRawBytes("Room.Known.List false")));
    QVERIFY(!known.getSnapshot().listComplete);
    QVERIFY(known.receive(GmcpMessage::fromRawBytes("Room.Known.Updated")));
    QVERIFY(!known.receive(GmcpMessage::fromRawBytes("Room.Known.List null")));
    QVERIFY(known.getSnapshot().listComplete); // a completed empty list is authoritative
    QVERIFY(!known.getSnapshot().isKnown(ServerRoomId{1}));
    QVERIFY(!known.setEnabled(false));
    QVERIFY(!known.getSnapshot().listComplete);
    QVERIFY(known.setEnabled(true));
    QVERIFY(!known.setEnabled(false));
    QVERIFY(!known.receive(GmcpMessage::fromRawBytes("Room.Known.List [1]")));
    QVERIFY(!known.receive(GmcpMessage::fromRawBytes("Room.Known.List null")));
    QVERIFY(!known.getSnapshot().listComplete);
    QVERIFY(known.setEnabled(true));
}

void TestProxy::knownRoomsMalformedTest()
{
    for (const auto *body : {"[1,-2]",
                             "[1,0]",
                             "[1,1.5]",
                             "[1,4294967296]",
                             "[true]",
                             "[null]",
                             "[\"2\"]",
                             "{}",
                             "true",
                             "[broken"}) {
        KnownRooms known;
        QVERIFY(!known.setEnabled(true));
        QVERIFY(known.receive(GmcpMessage::fromRawBytes(R"(Char.Name {"name":"Bilbo"})")));
        QVERIFY(!known.receive(GmcpMessage::fromRawBytes(QByteArray("Room.Known.List ") + body)));
        QVERIFY(!known.receive(GmcpMessage::fromRawBytes("Room.Known.List null")));
        QVERIFY(!known.getSnapshot().listComplete);
    }
    KnownRooms known;
    QVERIFY(!known.setEnabled(true));
    QVERIFY(known.receive(GmcpMessage::fromRawBytes(R"(Char.Name {"name":"Bilbo"})")));
    QVERIFY(!known.receive(GmcpMessage::fromRawBytes("Room.Known.List [4294967295]")));
    QVERIFY(!known.receive(GmcpMessage::fromRawBytes("Room.Known.List null")));
    for (const auto *body : {"-1", "0", "1.5", "4294967296", "42junk", "[42]"}) {
        const auto revision = known.getSnapshot().revision;
        QVERIFY(!known.receive(GmcpMessage::fromRawBytes(QByteArray("Room.Known.Add ") + body)));
        QCOMPARE(known.getSnapshot().revision, revision);
    }
    QVERIFY(known.getSnapshot().isKnown(ServerRoomId{UINT32_MAX}));
    QVERIFY(!known.getSnapshot().isKnown(ServerRoomId{42}));
}

void TestProxy::knownRoomsRefreshTest()
{
    KnownRooms known;
    QVERIFY(!known.setEnabled(true));
    QVERIFY(known.receive(GmcpMessage::fromRawBytes(R"(Char.Name {"name":"Bilbo"})")));
    QVERIFY(!known.receive(GmcpMessage::fromRawBytes("Room.Known.List [1]")));
    QVERIFY(!known.receive(GmcpMessage::fromRawBytes("Room.Known.Updated")));
    QVERIFY(!known.receive(GmcpMessage::fromRawBytes("Room.Known.Updated")));
    QVERIFY(known.receive(GmcpMessage::fromRawBytes("Room.Known.List null")));
    QVERIFY(!known.getSnapshot().listComplete);
    QVERIFY(!known.receive(GmcpMessage::fromRawBytes("Room.Known.List [2]")));
    QVERIFY(!known.receive(GmcpMessage::fromRawBytes("Room.Known.List null")));
    QVERIFY(!known.getSnapshot().isKnown(ServerRoomId{1}));
    QVERIFY(known.getSnapshot().isKnown(ServerRoomId{2}));
    QVERIFY(known.receive(GmcpMessage::fromRawBytes("Room.Known.Updated")));
    QVERIFY(!known.getSnapshot().listComplete);
    QVERIFY(!known.receive(GmcpMessage::fromRawBytes("Room.Known.List [3]")));
    QVERIFY(!known.receive(GmcpMessage::fromRawBytes(R"(Char.Name {"name":""})")));
    QVERIFY(!known.receive(GmcpMessage::fromRawBytes("Room.Known.List null")));
    QVERIFY(!known.getSnapshot().listComplete);
}

void TestProxy::knownRoomsTrackerTest()
{
    const auto gmcp = [](const char *text) { return GmcpMessage::fromRawBytes(text); };
    const auto none = []() -> std::optional<KnownRoomsTracker::StoreLocation> {
        return std::nullopt;
    };

    // Disabled by default: it must not ask the server for anything.
    {
        KnownRoomsTracker idle{nullptr, none};
        QStringList sent;
        QObject::connect(&idle, &KnownRoomsTracker::sig_sendGmcp, &idle, [&sent](const auto &msg) {
            sent << QString::fromUtf8(msg.toRawBytes());
        });
        idle.onConnected();
        idle.slot_parseGmcpInput(gmcp(R"(Char.Name {"name":"Bilbo"})"));
        QVERIFY(sent.isEmpty());
        QVERIFY(!idle.isEnabled());
    }

    KnownRoomsTracker tracker{nullptr, none};
    QStringList sent;
    int changes = 0;
    QObject::connect(&tracker, &KnownRoomsTracker::sig_sendGmcp, &tracker, [&sent](const auto &msg) {
        sent << QString::fromUtf8(msg.toRawBytes());
    });
    QObject::connect(&tracker, &KnownRoomsTracker::sig_changed, &tracker, [&changes]() {
        ++changes;
    });
    tracker.setEnabled(true);
    tracker.onConnected();
    tracker.slot_parseGmcpInput(gmcp(R"(Char.Name {"name":"Bilbo"})"));
    // The module is negotiated on demand, immediately followed by one list request.
    QCOMPARE(sent.size(), 2);
    QVERIFY(sent.at(0).startsWith("Core.Supports.Add"));
    QVERIFY(sent.at(0).contains("Room.Known 1"));
    QCOMPARE(sent.at(1), QString("Room.Known.List"));

    const auto beforeList = changes;
    tracker.slot_parseGmcpInput(gmcp("Room.Known.List [1,2]"));
    QCOMPARE(changes, beforeList + 1); // chunks add rooms immediately
    QVERIFY(tracker.getSnapshot().isKnown(ServerRoomId{2}));
    tracker.slot_parseGmcpInput(gmcp("Room.Known.List null"));
    QCOMPARE(changes, beforeList + 2);
    QVERIFY(tracker.getSnapshot().listComplete);
    tracker.onDisconnected();
    QVERIFY(!tracker.getSnapshot().listComplete);
    QVERIFY(tracker.getSnapshot().rooms.empty());
    QCOMPARE(sent.size(), 2); // nothing further is requested while disconnected
}

namespace {
void seedKnownRooms(const QString &directory, const QByteArray &name, const QByteArray &ids)
{
    KnownRooms known;
    known.configureCache(directory, "mume.org:4242", []() {});
    QVERIFY(!known.setEnabled(true));
    QVERIFY(known.receive(GmcpMessage::fromRawBytes("Char.Name {\"name\":\"" + name + "\"}")));
    QVERIFY(!known.receive(GmcpMessage::fromRawBytes("Room.Known.List " + ids)));
    QVERIFY(!known.receive(GmcpMessage::fromRawBytes("Room.Known.List null")));
}
} // namespace

void TestProxy::knownRoomsPartialListTest()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    seedKnownRooms(dir.path(), "Bilbo", "[1,2]");

    KnownRooms known;
    known.configureCache(dir.path(), "mume.org:4242", []() {});
    QVERIFY(!known.setEnabled(true));
    QVERIFY(known.receive(GmcpMessage::fromRawBytes(R"(Char.Name {"name":"Bilbo"})")));
    // The cached history is usable at once, but it is not an authoritative list.
    QVERIFY(known.getSnapshot().isKnown(ServerRoomId{1}));
    QVERIFY(!known.getSnapshot().listComplete);
    // A partial list adds rooms but must not remove anything.
    QVERIFY(!known.receive(GmcpMessage::fromRawBytes("Room.Known.List [2,3]")));
    QVERIFY(known.getSnapshot().isKnown(ServerRoomId{3}));
    QVERIFY(known.getSnapshot().isKnown(ServerRoomId{1}));
    // A live add during the transfer survives the final replacement.
    QVERIFY(!known.receive(GmcpMessage::fromRawBytes("Room.Known.Add 8")));
    QVERIFY(!known.receive(GmcpMessage::fromRawBytes("Room.Known.List null")));
    QVERIFY(known.getSnapshot().listComplete);
    QVERIFY(!known.getSnapshot().isKnown(ServerRoomId{1})); // only a clean end removes IDs
    QVERIFY(known.getSnapshot().isKnown(ServerRoomId{2}));
    QVERIFY(known.getSnapshot().isKnown(ServerRoomId{3}));
    QVERIFY(known.getSnapshot().isKnown(ServerRoomId{8}));
}

void TestProxy::knownRoomsDamagedChunkTest()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    seedKnownRooms(dir.path(), "Bilbo", "[1,2]");

    KnownRooms known;
    known.configureCache(dir.path(), "mume.org:4242", []() {});
    QVERIFY(!known.setEnabled(true));
    QVERIFY(known.receive(GmcpMessage::fromRawBytes(R"(Char.Name {"name":"Bilbo"})")));
    QVERIFY(!known.receive(GmcpMessage::fromRawBytes("Room.Known.List [3,4]")));
    // A damaged chunk is ignored entirely, even the valid IDs before the bad one...
    QVERIFY(!known.receive(GmcpMessage::fromRawBytes("Room.Known.List [5,-1]")));
    QVERIFY(!known.getSnapshot().isKnown(ServerRoomId{5}));
    // ...earlier valid chunks stay known...
    QVERIFY(known.getSnapshot().isKnown(ServerRoomId{3}));
    QVERIFY(!known.receive(GmcpMessage::fromRawBytes("Room.Known.List null")));
    // ...and the list can no longer prove that cached rooms are stale.
    QVERIFY(!known.getSnapshot().listComplete);
    QVERIFY(known.getSnapshot().isKnown(ServerRoomId{1}));
    QVERIFY(known.getSnapshot().isKnown(ServerRoomId{3}));
}

void TestProxy::knownRoomsCacheMissingWarningTest()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    seedKnownRooms(dir.path(), "Bilbo", "[1,2]");
    const auto path = dir.path();
    KnownRoomsTracker observer{nullptr, [path]() {
                                   return std::optional<KnownRoomsTracker::StoreLocation>{
                                       {path, "mume.org:4242"}};
                               }};
    int warnings = 0;
    QObject::connect(&observer, &KnownRoomsTracker::sig_cacheMissing, &observer, [&warnings]() {
        ++warnings;
    });
    observer.setEnabled(true);
    observer.slot_parseGmcpInput(GmcpMessage::fromRawBytes(R"(Char.Name {"name":"Bilbo"})"));
    QVERIFY(observer.getSnapshot().isKnown(ServerRoomId{1})); // restored from the cache
    QCOMPARE(warnings, 0);
    observer.slot_parseGmcpInput(GmcpMessage::fromRawBytes(R"(Char.Name {"name":"Frodo"})"));
    QCOMPARE(warnings, 1);
    observer.slot_parseGmcpInput(GmcpMessage::fromRawBytes(R"(Char.Name {"name":"Frodo"})"));
    QCOMPARE(warnings, 1);
    // Reads are synchronous, so a character without a cache warns immediately, once.
    observer.onDisconnected();
    observer.slot_parseGmcpInput(GmcpMessage::fromRawBytes(R"(Char.Name {"name":"Sam"})"));
    QCOMPARE(warnings, 2);
    observer.slot_parseGmcpInput(GmcpMessage::fromRawBytes("Room.Known.List null"));
    QCOMPARE(warnings, 2);
    observer.onDisconnected();
    observer.setEnabled(false);
    observer.slot_parseGmcpInput(GmcpMessage::fromRawBytes(R"(Char.Name {"name":"Merry"})"));
    QCOMPARE(warnings, 2);
    // Switching to a character that does have a cache never warns for the earlier one.
    observer.setEnabled(true);
    observer.onDisconnected();
    observer.slot_parseGmcpInput(GmcpMessage::fromRawBytes(R"(Char.Name {"name":"Pippin"})"));
    QCOMPARE(warnings, 3);
    observer.slot_parseGmcpInput(GmcpMessage::fromRawBytes(R"(Char.Name {"name":"Bilbo"})"));
    QVERIFY(observer.getSnapshot().isKnown(ServerRoomId{1})); // cache hit
    QCOMPARE(warnings, 3);
}

void TestProxy::knownRoomsCacheRestartTest()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    seedKnownRooms(dir.path(), "Bilbo", "[1,2]");
    int changed = 0;
    {
        KnownRooms known;
        known.configureCache(dir.path(), "mume.org:4242", [&changed]() { ++changed; });
        QVERIFY(!known.setEnabled(true));
        QVERIFY(known.receive(GmcpMessage::fromRawBytes(R"(Char.Name {"name":"Bilbo"})")));
        QVERIFY(!known.getSnapshot().rooms.empty());
        QVERIFY(changed > 0);
        QVERIFY(known.getSnapshot().isKnown(ServerRoomId{1}));
        // Keep the cached view during the slow refresh, then allow truth to remove stale IDs.
        QVERIFY(!known.receive(GmcpMessage::fromRawBytes("Room.Known.List [2,3]")));
        QVERIFY(known.getSnapshot().isKnown(ServerRoomId{1}));
        QVERIFY(!known.receive(GmcpMessage::fromRawBytes("Room.Known.List null")));
        QVERIFY(!known.getSnapshot().isKnown(ServerRoomId{1}));
        QVERIFY(known.getSnapshot().isKnown(ServerRoomId{3}));
    }
    KnownRooms next;
    next.configureCache(dir.path(), "mume.org:4242", []() {});
    QVERIFY(!next.setEnabled(true));
    QVERIFY(next.receive(GmcpMessage::fromRawBytes(R"(Char.Name {"name":"Bilbo"})")));
    QVERIFY(!next.getSnapshot().rooms.empty());
    QVERIFY(!next.getSnapshot().isKnown(ServerRoomId{1}));
    QVERIFY(next.getSnapshot().isKnown(ServerRoomId{3}));
}

void TestProxy::knownRoomsCacheLiveTest()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    seedKnownRooms(dir.path(), "Bilbo", "[1,2]");
    {
        KnownRooms known;
        known.configureCache(dir.path(), "mume.org:4242", []() {});
        QVERIFY(!known.setEnabled(true));
        QVERIFY(known.receive(GmcpMessage::fromRawBytes(R"(Char.Name {"name":"Bilbo"})")));
        // A discovery arriving before the disk read completes must not be lost.
        QVERIFY(!known.receive(GmcpMessage::fromRawBytes("Room.Known.Add 9")));
        QVERIFY(!known.getSnapshot().rooms.empty());
        QVERIFY(known.getSnapshot().isKnown(ServerRoomId{9}));
        QVERIFY(!known.receive(GmcpMessage::fromRawBytes("Room.Known.List [1]")));
        QVERIFY(!known.receive(GmcpMessage::fromRawBytes("Room.Known.Add 10")));
        QVERIFY(!known.receive(GmcpMessage::fromRawBytes("Room.Known.List null")));
        QVERIFY(known.getSnapshot().isKnown(ServerRoomId{9}));
        QVERIFY(known.getSnapshot().isKnown(ServerRoomId{10}));
        QVERIFY(!known.getSnapshot().isKnown(ServerRoomId{2}));
        QVERIFY(!known.setEnabled(false));
        QVERIFY(!known.receive(GmcpMessage::fromRawBytes("Room.Known.Add 11")));
        // Continuous updates are coalesced; ordinary shutdown still saves the last one.
        for (int id = 20; id < 120; ++id) {
            QVERIFY(!known.receive(
                GmcpMessage::fromRawBytes("Room.Known.Add " + QByteArray::number(id))));
        }
    }
    KnownRooms next;
    next.configureCache(dir.path(), "mume.org:4242", []() {});
    QVERIFY(!next.setEnabled(true));
    QVERIFY(next.receive(GmcpMessage::fromRawBytes(R"(Char.Name {"name":"Bilbo"})")));
    QVERIFY(!next.getSnapshot().rooms.empty());
    QCOMPARE(next.getSnapshot().rooms.size(), size_t{104});
    QVERIFY(next.getSnapshot().isKnown(ServerRoomId{11}));
    QVERIFY(next.getSnapshot().isKnown(ServerRoomId{119}));
}

void TestProxy::knownRoomsCacheInterruptedTest()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    seedKnownRooms(dir.path(), "Bilbo", "[1,2]");
    {
        KnownRooms known;
        known.configureCache(dir.path(), "mume.org:4242", []() {});
        QVERIFY(!known.setEnabled(true));
        QVERIFY(known.receive(GmcpMessage::fromRawBytes(R"(Char.Name {"name":"Bilbo"})")));
        QVERIFY(!known.getSnapshot().rooms.empty());
        QVERIFY(!known.receive(GmcpMessage::fromRawBytes("Room.Known.List [3]")));
        QVERIFY(!known.receive(GmcpMessage::fromRawBytes("Room.Known.List false")));
        QVERIFY(known.getSnapshot().isKnown(ServerRoomId{1}));
        // Rooms from the interrupted list were still real discoveries.
        QVERIFY(known.getSnapshot().isKnown(ServerRoomId{3}));
        QVERIFY(!known.getSnapshot().listComplete);
    }
    KnownRooms next;
    next.configureCache(dir.path(), "mume.org:4242", []() {});
    QVERIFY(!next.setEnabled(true));
    QVERIFY(next.receive(GmcpMessage::fromRawBytes(R"(Char.Name {"name":"Bilbo"})")));
    // The good cache was extended, never replaced by the interrupted list.
    QCOMPARE(next.getSnapshot().rooms.size(), size_t{3});
    QVERIFY(next.getSnapshot().isKnown(ServerRoomId{1}));
}

void TestProxy::knownRoomsCacheIdentityTest()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    seedKnownRooms(dir.path(), "Bilbo", "[1]");
    seedKnownRooms(dir.path(), "Frodo", "[2]");
    KnownRooms known;
    known.configureCache(dir.path(), "mume.org:4242", []() {});
    QVERIFY(!known.setEnabled(true));
    QVERIFY(known.receive(GmcpMessage::fromRawBytes(R"(Char.Name {"name":"Bilbo"})")));
    QVERIFY(!known.receive(GmcpMessage::fromRawBytes(R"(Char.Name {"name":"Frodo"})")));
    QVERIFY(!known.getSnapshot().rooms.empty());
    QVERIFY(!known.getSnapshot().isKnown(ServerRoomId{1}));
    QVERIFY(known.getSnapshot().isKnown(ServerRoomId{2}));
    QVERIFY(known.receive(GmcpMessage::fromRawBytes("Room.Known.List null")));
    QVERIFY(!known.receive(GmcpMessage::fromRawBytes("Room.Known.List [2]")));
    QVERIFY(!known.receive(GmcpMessage::fromRawBytes("Room.Known.List null")));
    known.reset();
    QVERIFY(!known.getSnapshot().listComplete);
    KnownRoomsStore other(dir.path(), "another-server:4242");
    QVERIFY(!other.load("Bilbo"));
}

void TestProxy::knownRoomsCacheServerSwitchTest()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    seedKnownRooms(dir.path(), "Bilbo", "[1,2]");
    {
        KnownRooms known;
        known.configureCache(dir.path(), "mume.org:4242", []() {});
        QVERIFY(!known.setEnabled(true));
        QVERIFY(known.receive(GmcpMessage::fromRawBytes(R"(Char.Name {"name":"Bilbo"})")));
        QVERIFY(!known.getSnapshot().rooms.empty());
        QVERIFY(!known.receive(GmcpMessage::fromRawBytes("Room.Known.Add 3")));
        // Replacing a server retains its pending I/O and final updates until teardown.
        known.configureCache(dir.path(), "other.example:4242", []() {});
        QVERIFY(!known.getSnapshot().listComplete);
        QVERIFY(known.receive(GmcpMessage::fromRawBytes(R"(Char.Name {"name":"Bilbo"})")));
        QVERIFY(!known.receive(GmcpMessage::fromRawBytes("Room.Known.List [99]")));
        QVERIFY(!known.receive(GmcpMessage::fromRawBytes("Room.Known.List null")));
        // Immediately returning must read the original server's queued newer snapshot.
        known.configureCache(dir.path(), "mume.org:4242", []() {});
        QVERIFY(known.receive(GmcpMessage::fromRawBytes(R"(Char.Name {"name":"Bilbo"})")));
        QVERIFY(!known.getSnapshot().rooms.empty());
        QVERIFY(known.getSnapshot().isKnown(ServerRoomId{3}));
        QVERIFY(!known.getSnapshot().isKnown(ServerRoomId{99}));
    }
    KnownRooms original;
    original.configureCache(dir.path(), "mume.org:4242", []() {});
    QVERIFY(!original.setEnabled(true));
    QVERIFY(original.receive(GmcpMessage::fromRawBytes(R"(Char.Name {"name":"Bilbo"})")));
    QVERIFY(!original.getSnapshot().rooms.empty());
    QCOMPARE(original.getSnapshot().rooms.size(), size_t{3});
    QVERIFY(original.getSnapshot().isKnown(ServerRoomId{3}));
    QVERIFY(!original.getSnapshot().isKnown(ServerRoomId{99}));
}

void TestProxy::knownRoomsCacheCaseFoldTest()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    KnownRoomsStore::Rooms first;
    first.insert(ServerRoomId{1});
    auto latest = first;
    latest.insert(ServerRoomId{2});
    KnownRoomsStore store(dir.path(), "mume.org:4242");
    // Both spellings address the same file and must share the same pending write.
    store.save("Bilbo", first);
    store.save("bilbo", latest);
    const auto loaded = store.load("BILBO");
    QVERIFY(loaded.has_value());
    QVERIFY(*loaded == latest);
}

namespace {
QByteArray storeBytes(const QByteArray &server,
                      const QByteArray &character,
                      const std::vector<uint32_t> &ids,
                      const uint32_t version = 1,
                      const uint32_t declaredCount = UINT32_MAX,
                      const QByteArray &trailing = {})
{
    QByteArray bytes;
    QDataStream out(&bytes, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);
    out.setVersion(QDataStream::Qt_6_0);
    out << uint32_t{0x524B4D4D} << version << server << character
        << (declaredCount == UINT32_MAX ? static_cast<uint32_t>(ids.size()) : declaredCount);
    for (const auto id : ids) {
        out << id;
    }
    return bytes + trailing;
}
} // namespace

void TestProxy::knownRoomsCacheCorruptTest()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    seedKnownRooms(dir.path(), "Bilbo", "[1]");
    const auto files = QDir(dir.path()).entryList({"*.bin"}, QDir::Files);
    QCOMPARE(files.size(), qsizetype{1});

    const QByteArray server = "mume.org:4242";
    // A valid record first, so the harness itself is known to work.
    const auto good = storeBytes(server, "bilbo", {1, 5});
    for (const auto &contents : {QByteArray("garbage"),
                                 QByteArray(),
                                 storeBytes(server, "bilbo", {1}, 2),
                                 storeBytes(server, "frodo", {1}),
                                 storeBytes("other:1", "bilbo", {1}),
                                 storeBytes(server, "bilbo", {0, 1}),       // zero ID
                                 storeBytes(server, "bilbo", {2, 1}),       // unsorted
                                 storeBytes(server, "bilbo", {1, 1}),       // duplicate
                                 storeBytes(server, "bilbo", {1, 2}, 1, 3), // truncated
                                 storeBytes(server, "bilbo", {1, 2}, 1, 1), // padded count
                                 storeBytes(server, "bilbo", {1}, 1, 1, "x"),
                                 storeBytes(server, "bilbo", {}, 1, 4'000'000)}) {
        const auto path = QDir(dir.path()).filePath(files.front());
        for (const auto &candidate : {good, contents}) {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
            QCOMPARE(file.write(candidate), qint64{candidate.size()});
            file.close();
            KnownRoomsStore store(dir.path(), "mume.org:4242");
            const auto rooms = store.load("Bilbo");
            QCOMPARE(rooms.has_value(), candidate == good);
        }
    }
}

void TestProxy::knownRoomsCacheTruthWinsTest()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    seedKnownRooms(dir.path(), "Bilbo", "[1,2]");
    {
        KnownRooms known;
        known.configureCache(dir.path(), "mume.org:4242", []() {});
        QVERIFY(!known.setEnabled(true));
        QVERIFY(known.receive(GmcpMessage::fromRawBytes(R"(Char.Name {"name":"Bilbo"})")));
        // The cached view is available at once, and server truth then replaces it.
        QVERIFY(!known.receive(GmcpMessage::fromRawBytes("Room.Known.List [3]")));
        QVERIFY(!known.receive(GmcpMessage::fromRawBytes("Room.Known.List null")));
        QTest::qWait(100);
        QVERIFY(!known.getSnapshot().isKnown(ServerRoomId{1}));
        QVERIFY(known.getSnapshot().isKnown(ServerRoomId{3}));
        // Check a normal debounced save before shutdown, not only destructor flushing.
        const auto containsId = [&dir](const uint32_t id) {
            KnownRoomsStore reader(dir.path(), "mume.org:4242");
            const auto rooms = reader.load("Bilbo");
            return rooms && rooms->contains(ServerRoomId{id});
        };
        QTRY_VERIFY(containsId(3));
        QVERIFY(!known.receive(GmcpMessage::fromRawBytes("Room.Known.Add 4")));
        QTRY_VERIFY(containsId(4));
    }
    KnownRooms next;
    next.configureCache(dir.path(), "mume.org:4242", []() {});
    QVERIFY(!next.setEnabled(true));
    QVERIFY(next.receive(GmcpMessage::fromRawBytes(R"(Char.Name {"name":"Bilbo"})")));
    QVERIFY(!next.getSnapshot().rooms.empty());
    QCOMPARE(next.getSnapshot().rooms.size(), size_t{2});
    QVERIFY(next.getSnapshot().isKnown(ServerRoomId{4}));
}

QTEST_MAIN(TestProxy)
