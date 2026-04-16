// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "TestProxy.h"

#include "../src/global/TextUtils.h"
#include "../src/proxy/AbstractTelnet.h"
#include "../src/proxy/GmcpMessage.h"
#include "../src/proxy/GmcpModule.h"
#include "../src/proxy/GmcpUtils.h"
#include "../src/proxy/telnetfilter.h"

#include <QDebug>
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

class TestTelnet final : public AbstractTelnet
{
public:
    TestTelnet()
        : AbstractTelnet(TextCodecStrategyEnum::FORCE_UTF_8, TelnetTermTypeBytes{"test"})
    {}
    ~TestTelnet() override;

    void virt_sendRawData(const TelnetIacBytes &data) override { lastRaw = data; }
    void virt_sendToMapper(const RawBytes & /*data*/, bool /*goAhead*/) override {}
    void virt_receiveNewEnvironSend(const QList<RawBytes> &vars,
                                    const QList<RawBytes> &userVars) override
    {
        receivedVars = vars;
        receivedUserVars = userVars;
        receivedType = 1; // SEND
    }
    void virt_receiveNewEnvironIs(const QMap<RawBytes, RawBytes> &vars,
                                  const QMap<RawBytes, RawBytes> &userVars) override
    {
        receivedIsVars = vars;
        receivedIsUserVars = userVars;
        receivedType = 0; // IS
    }
    void virt_receiveNewEnvironInfo(const QMap<RawBytes, RawBytes> &vars,
                                    const QMap<RawBytes, RawBytes> &userVars) override
    {
        receivedIsVars = vars;
        receivedIsUserVars = userVars;
        receivedType = 2; // INFO
    }

    void onRead(const QByteArray &data) { onReadInternal(TelnetIacBytes{data}); }
    void enableNewEnviron() { m_options.myOptionState[OPT_NEW_ENVIRON] = true; }

    TelnetIacBytes lastRaw;
    QList<RawBytes> receivedVars;
    QList<RawBytes> receivedUserVars;
    QMap<RawBytes, RawBytes> receivedIsVars;
    QMap<RawBytes, RawBytes> receivedIsUserVars;
    int receivedType = -1;
};

TestTelnet::~TestTelnet() = default;

void TestProxy::mnesTest()
{
    TestTelnet telnet;
    telnet.enableNewEnviron();

    // Test SEND ALL
    // IAC SB NEW-ENVIRON SEND IAC SE -> 255 250 39 1 255 240
    QByteArray sendAll;
    sendAll.append(static_cast<char>(255u));
    sendAll.append(static_cast<char>(250u));
    sendAll.append(static_cast<char>(39u));
    sendAll.append(static_cast<char>(1u));
    sendAll.append(static_cast<char>(255u));
    sendAll.append(static_cast<char>(240u));
    telnet.onRead(sendAll);
    QCOMPARE(telnet.receivedType, 1);
    QVERIFY(telnet.receivedVars.isEmpty());
    QVERIFY(telnet.receivedUserVars.isEmpty());

    // Test SEND VAR
    // IAC SB NEW-ENVIRON SEND VAR "MTTS" IAC SE
    QByteArray sendVar;
    sendVar.append(static_cast<char>(255u));
    sendVar.append(static_cast<char>(250u));
    sendVar.append(static_cast<char>(39u));
    sendVar.append(static_cast<char>(1u));
    sendVar.append(static_cast<char>(0u));
    sendVar.append("MTTS");
    sendVar.append(static_cast<char>(255u));
    sendVar.append(static_cast<char>(240u));
    telnet.onRead(sendVar);
    QCOMPARE(telnet.receivedType, 1);
    QCOMPARE(telnet.receivedVars.size(), 1);
    QCOMPARE(telnet.receivedVars[0].getQByteArray(), QByteArray("MTTS"));

    // Test IS
    // IAC SB NEW-ENVIRON IS VAR "CHARSET" VAL "UTF-8" IAC SE
    QByteArray isVar;
    isVar.append(static_cast<char>(255u));
    isVar.append(static_cast<char>(250u));
    isVar.append(static_cast<char>(39u));
    isVar.append(static_cast<char>(0u));
    isVar.append(static_cast<char>(0u));
    isVar.append("CHARSET");
    isVar.append(static_cast<char>(1u));
    isVar.append("UTF-8");
    isVar.append(static_cast<char>(255u));
    isVar.append(static_cast<char>(240u));
    telnet.onRead(isVar);
    QCOMPARE(telnet.receivedType, 0);
    QCOMPARE(telnet.receivedIsVars.size(), 1);
    QCOMPARE(telnet.receivedIsVars[RawBytes("CHARSET")].getQByteArray(), QByteArray("UTF-8"));
}

QTEST_MAIN(TestProxy)
