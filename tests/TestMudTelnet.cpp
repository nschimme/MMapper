// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include "TestMudTelnet.h"

#include "../src/proxy/AbstractTelnet.h"
#include "../src/proxy/MudTelnet.h"
#include <QSignalSpy>

#include <QJsonDocument>
#include <QJsonObject>

#include <memory>

namespace {
class MockMudTelnetOutputs : public MudTelnetOutputs
{
public:
    QByteArray sentData;

private:
    void virt_onAnalyzeMudStream(const RawBytes &, bool) final {}
    void virt_onSendToSocket(const TelnetIacBytes &bytes) final
    {
        sentData.append(bytes.getQByteArray());
    }
    void virt_onRelayEchoMode(bool) final {}
    void virt_onRelayGmcpFromMudToUser(const GmcpMessage &) final {}
    void virt_onSendMSSPToUser(const TelnetMsspBytes &) final {}
    void virt_onSendGameTimeToClock(const MsspTime &) final {}
    void virt_onTryCharLogin() final {}
    void virt_onMumeClientView(const QString &, const QString &) final {}
    void virt_onMumeClientEdit(const RemoteSessionId, const QString &, const QString &) final {}
    void virt_onMumeClientError(const QString &) final {}
};
} // namespace

void TestMudTelnet::coreHelloTest()
{
    MockMudTelnetOutputs outputs;
    MudTelnet mudTelnet(outputs);

    mudTelnet.onRelayTermType(TelnetTermTypeBytes{"MMapper-Test"});
    mudTelnet.onRelayCharset(CharacterEncodingEnum::UTF8);

    // Simulate MUME enabling GMCP
    auto commandBytes = std::make_unique<uint8_t[]>(3);
    commandBytes[0] = TN_IAC;
    commandBytes[1] = TN_WILL;
    commandBytes[2] = OPT_GMCP;

    const QByteArray gmcpEnableCommand =
        QByteArray::fromRawData(reinterpret_cast<const char *>(commandBytes.get()), 3);
    mudTelnet.onAnalyzeMudStream(TelnetIacBytes(gmcpEnableCommand));

    // Extract the GMCP message from the raw bytes sent to the socket
    QByteArray gmcpPayload;
    int iacPos = outputs.sentData.indexOf(static_cast<char>(TN_IAC));
    QVERIFY(iacPos != -1);

    int sbPos = outputs.sentData.indexOf(static_cast<char>(TN_SB), iacPos);
    QVERIFY(sbPos != -1);

    int sePos = outputs.sentData.indexOf(static_cast<char>(TN_SE), sbPos);
    QVERIFY(sePos != -1);

    gmcpPayload = outputs.sentData.mid(sbPos + 2, sePos - sbPos - 3);

    GmcpMessage msg = GmcpMessage::fromRawBytes(gmcpPayload);
    QCOMPARE(msg.getName().toQString(), QString("Core.Hello"));

    QJsonDocument doc = QJsonDocument::fromJson(msg.getJson()->toQByteArray());
    QJsonObject obj = doc.object();

    QVERIFY(obj.contains("client"));
    QCOMPARE(obj["client"].toString(), QString("MMapper"));
    QVERIFY(obj.contains("version"));
    QVERIFY(obj.contains("os"));
    QVERIFY(obj.contains("arch"));
    QVERIFY(obj.contains("package"));
    QVERIFY(obj.contains("terminalType"));
    QCOMPARE(obj["terminalType"].toString(), QString("MMapper-Test"));
    QVERIFY(obj.contains("charset"));
    QCOMPARE(obj["charset"].toString(), QString("UTF-8"));
}

QTEST_MAIN(TestMudTelnet)
