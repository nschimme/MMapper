// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "TestProxy.h"

#include "../src/global/TextUtils.h"
#include "../src/proxy/GmcpMessage.h"
#include "../src/proxy/GmcpModule.h"
#include "../src/proxy/GmcpUtils.h"
#include "../src/proxy/MudTelnet.h"
#include "../src/proxy/TelnetConsts.h"
#include "../src/proxy/telnetfilter.h"

#include <QDebug>
#include <QSignalSpy>
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

// Mock implementation for MudTelnetOutputs to intercept socket data
class MockMudTelnetOutputs final : public MudTelnetOutputs
{
public:
    QList<TelnetIacBytes> m_sentData;

    void virt_onAnalyzeMudStream(const RawBytes &, bool) override {}
    void virt_onSendToSocket(const TelnetIacBytes &bytes) override { m_sentData.append(bytes); }
    void virt_onRelayEchoMode(bool) override {}
    void virt_onRelayGmcpFromMudToUser(const GmcpMessage &) override {}
    void virt_onSendMSSPToUser(const TelnetMsspBytes &) override {}
    void virt_onSendGameTimeToClock(const MsspTime &) override {}
    void virt_onTryCharLogin() override {}
    void virt_onMumeClientView(const QString &, const QString &) override {}
    void virt_onMumeClientEdit(const RemoteSessionId, const QString &, const QString &) override
    {
    }
    void virt_onMumeClientError(const QString &) override {}
};

void TestProxy::mnesNegotiationTest_data()
{
    QTest::addColumn<TelnetIacBytes>("input");
    QTest::addColumn<TelnetIacBytes>("expectedOutput");
    QTest::addColumn<bool>("expectWillNewEnviron"); // True if we expect IAC WILL NEW-ENVIRON

    // Test case 1: Server sends IAC DO NEW-ENVIRON
    QTest::newRow("Server requests NEW-ENVIRON")
        << TelnetIacBytes{TN_IAC, TN_DO, TN_NEW_ENVIRON} // Input
        << TelnetIacBytes{TN_IAC, TN_WILL, TN_NEW_ENVIRON, // Expected: We agree
                          TN_IAC, TN_SB,   TN_NEW_ENVIRON, TNSB_NEWENV_IS, TNSB_NEWENV_VAR,
                          'C',    'L',     'I',            'E',            'N',
                          'T',    '_',     'N',            'A',            'M',
                          'E',    TNSB_NEWENV_VAL, 'M',    'M',            'a',
                          'p',    'p',     'e',            'r',            TNSB_NEWENV_VAR,
                          'C',    'L',     'I',            'E',            'N',
                          'T',    '_',     'V',            'E',            'R',
                          'S',    'I',     'O',            'N',            TNSB_NEWENV_VAL,
                          // Version will be dynamic, so we check for its presence later
                          TNSB_NEWENV_VAR, 'C',    'H',     'A',            'R',
                          'S',    'E',     'T',    TNSB_NEWENV_VAL, 'U',    'T',
                          'F',    '-',     '8',    TN_IAC,         TN_SE}
        << true;

    // Test case 2: Server sends IAC WILL NEW-ENVIRON (Client should not initiate, but respond if server does)
    // This scenario is less common for MNES as server usually initiates with DO.
    // However, the underlying Telnet option handling should still process it.
    // For MNES, client doesn't send data on WILL from server, only after DO from server and WILL from client.
    QTest::newRow("Server WILLS NEW-ENVIRON")
        << TelnetIacBytes{TN_IAC, TN_WILL, TN_NEW_ENVIRON} // Input
        << TelnetIacBytes{}                                // Expected: No response based on current MNES logic for client
        << false;

    // Test case 3: Server sends IAC DONT NEW-ENVIRON
    QTest::newRow("Server DONT NEW-ENVIRON")
        << TelnetIacBytes{TN_IAC, TN_DONT, TN_NEW_ENVIRON} // Input
        << TelnetIacBytes{TN_IAC, TN_WONT, TN_NEW_ENVIRON} // Expected: We acknowledge
        << false;

    // Test case 4: Server sends IAC WONT NEW-ENVIRON
    QTest::newRow("Server WONT NEW-ENVIRON")
        << TelnetIacBytes{TN_IAC, TN_WONT, TN_NEW_ENVIRON} // Input
        << TelnetIacBytes{} // Expected: No response, client doesn't initiate WONT unless it previously sent WILL
        << false;
}

void TestProxy::mnesNegotiationTest()
{
    QFETCH(TelnetIacBytes, input);
    QFETCH(TelnetIacBytes, expectedOutput);
    QFETCH(bool, expectWillNewEnviron);

    MockMudTelnetOutputs outputs;
    MudTelnet mudTelnet(outputs);
    mudTelnet.setDebug(true); // Enable debug for more verbose output if needed

    // Simulate receiving the IAC sequence from the server
    mudTelnet.onAnalyzeMudStream(input);

    if (outputs.m_sentData.isEmpty()) {
        QVERIFY(expectedOutput.isEmpty());
        return;
    }

    // For the server DO NEW-ENVIRON case, there are two packets sent:
    // 1. IAC WILL NEW-ENVIRON
    // 2. IAC SB NEW-ENVIRON IS VAR "CLIENT_NAME" VAL "MMapper" ... IAC SE
    // We need to combine them for comparison or check them separately.
    // For simplicity in this test, we'll check the first part (WILL)
    // and then the structure of the second part (SB ... SE).

    if (expectWillNewEnviron) {
        QVERIFY(outputs.m_sentData.size() >= 1);
        TelnetIacBytes firstResponse = outputs.m_sentData.first();
        TelnetIacBytes expectedWill{TN_IAC, TN_WILL, TN_NEW_ENVIRON};
        QCOMPARE(firstResponse, expectedWill);

        if (outputs.m_sentData.size() > 1) {
            TelnetIacBytes secondResponse = outputs.m_sentData.at(1);
            // Verify the structure of the IS response
            QVERIFY(secondResponse.startsWith(QByteArray{TN_IAC, TN_SB, TN_NEW_ENVIRON, TNSB_NEWENV_IS}));
            QVERIFY(secondResponse.endsWith(QByteArray{TN_IAC, TN_SE}));
            QVERIFY(secondResponse.contains(QByteArray("CLIENT_NAME") + TNSB_NEWENV_VAL + QByteArray("MMapper")));
            QVERIFY(secondResponse.contains(QByteArray("CLIENT_VERSION") + TNSB_NEWENV_VAL)); // Check for key + VAL
            QVERIFY(secondResponse.contains(QByteArray("CHARSET") + TNSB_NEWENV_VAL + QByteArray("UTF-8")));
        } else {
            // This case might happen if only WILL was expected and no subsequent IS
            QVERIFY(expectedOutput.startsWith(QByteArray{TN_IAC, TN_WILL, TN_NEW_ENVIRON}) && expectedOutput.size() == 3);
        }
    } else {
        if (expectedOutput.isEmpty()) {
            QVERIFY(outputs.m_sentData.isEmpty() || outputs.m_sentData.first().isEmpty());
        } else {
            QVERIFY(outputs.m_sentData.size() == 1);
            QCOMPARE(outputs.m_sentData.first(), expectedOutput);
        }
    }
}

QTEST_MAIN(TestProxy)
