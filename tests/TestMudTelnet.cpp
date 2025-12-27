// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include "../src/proxy/MudTelnet.h"
#include <QtTest/QtTest>
#include <QObject>

class TestMudTelnet : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void gmcpCoreHelloTest();
};

namespace { // anonymous
class MockMudTelnetOutputs final : public MudTelnetOutputs
{
public:
    QList<TelnetIacBytes> captured;

private:
    void virt_onAnalyzeMudStream(const RawBytes &, bool) final {}
    void virt_onSendToSocket(const TelnetIacBytes &data) final { captured.append(data); }
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

void TestMudTelnet::gmcpCoreHelloTest()
{
    MockMudTelnetOutputs outputs;
    MudTelnet telnet(outputs);
    telnet.virt_onGmcpEnabled();

    std::unique_ptr<GmcpMessage> gmcp;
    for (const auto &cap : outputs.captured) {
        auto msg = std::make_unique<GmcpMessage>(GmcpMessage::fromRawBytes(cap.getQByteArray().mid(3, cap.size() - 5)));
        if (msg->isCoreHello()) {
            gmcp = std::move(msg);
            break;
        }
    }

    QVERIFY(gmcp);
    QVERIFY(gmcp->isCoreHello());
    const auto doc = gmcp->getJsonDocument();
    QVERIFY(doc.has_value());
    const auto obj = doc->getObject();
    QVERIFY(obj.has_value());
    QCOMPARE(obj->getString("charset"), "UTF-8");
}

QTEST_MAIN(TestMudTelnet)
#include "TestMudTelnet.moc"
