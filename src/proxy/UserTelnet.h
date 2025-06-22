#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "../global/Signal2.h"
#include "AbstractTelnet.h"

#include <QByteArray>
#include <QObject>

struct NODISCARD UserTelnetOutputs
{
public:
    virtual ~UserTelnetOutputs();

public:
    void onAnalyzeUserStream(const RawBytes &bytes, const bool goAhead)
    {
        virt_onAnalyzeUserStream(bytes, goAhead);
    }
    void onSendToSocket(const TelnetIacBytes &bytes) { virt_onSendToSocket(bytes); }
    void onRelayGmcpFromUserToMud(const GmcpMessage &msg) { virt_onRelayGmcpFromUserToMud(msg); }
    void onRelayNawsFromUserToMud(const int width, const int height)
    {
        virt_onRelayNawsFromUserToMud(width, height);
    }
    void onRelayTermTypeFromUserToMud(const TelnetTermTypeBytes &bytes)
    {
        virt_onRelayTermTypeFromUserToMud(bytes);
    }
    void onClientEnvironVariableReceived(const QString &key, const QString &value)
    {
        virt_onClientEnvironVariableReceived(key, value);
    }
    void onClientMttsValueReceived(const QString &mttsValue)
    {
        virt_onClientMttsValueReceived(mttsValue);
    }
    void onClientTerminalNameReceived(const TelnetTermTypeBytes &terminalName)
    {
        virt_onClientTerminalNameReceived(terminalName);
    }

private:
    virtual void virt_onAnalyzeUserStream(const RawBytes &, bool) = 0;
    virtual void virt_onSendToSocket(const TelnetIacBytes &) = 0;
    virtual void virt_onRelayGmcpFromUserToMud(const GmcpMessage &) = 0;
    virtual void virt_onRelayNawsFromUserToMud(int, int) = 0;
    virtual void virt_onRelayTermTypeFromUserToMud(const TelnetTermTypeBytes &) = 0; // For the first TTYPE (client name)
    virtual void virt_onClientTerminalNameReceived(const TelnetTermTypeBytes &terminalName) = 0; // For the second TTYPE (terminal name)
    virtual void virt_onClientEnvironVariableReceived(const QString &key, const QString &value) = 0;
    virtual void virt_onClientMttsValueReceived(const QString &mttsValue) = 0;
};

class NODISCARD UserTelnet final : public AbstractTelnet
{
private:
    /** modules for GMCP */
    struct NODISCARD GmcpData final
    {
        /** MMapper relevant modules and their version */
        GmcpModuleVersionList supported;
        /** All GMCP modules */
        GmcpModuleSet modules;
    } m_gmcp{};

private:
    UserTelnetOutputs &m_outputs;

public:
    explicit UserTelnet(UserTelnetOutputs &outputs);
    ~UserTelnet() final = default;

private:
    NODISCARD bool virt_isGmcpModuleEnabled(const GmcpModuleTypeEnum &name) const final;
    void virt_sendToMapper(const RawBytes &data, bool goAhead) final;
    void virt_receiveGmcpMessage(const GmcpMessage &) final;
    void virt_receiveTerminalType(const TelnetTermTypeBytes &) final;
    void virt_receiveWindowSize(int, int) final;
    void virt_onNewEnvironEnabledByPeer() final;
    void virt_onTerminalTypeEnabledByPeer() final;
    void virt_receiveNewEnvironIs(const QByteArray &data) final;
    void virt_receiveNewEnvironSend(const QByteArray &data) final;
    void virt_receiveNewEnvironInfo(const QByteArray &data) final;
    void virt_sendRawData(const TelnetIacBytes &data) final;

private:
    // MNES specific data received from client
    QString m_clientIpAddress;
    // We will need to store other client-provided MNES variables too, like MTTS, CHARSET etc.
    // For now, IP address is a key one for proxying.
    std::map<QString, QString> m_clientEnvironVariables;

    enum class TtypeState {
        Idle,
        AwaitingClientName,
        AwaitingTerminalName,
        AwaitingMtts,
        AwaitingMttsConfirm,
        Complete
    };
    TtypeState m_ttypeState = TtypeState::Idle;
    TelnetTermTypeBytes m_clientReportedName;
    TelnetTermTypeBytes m_clientReportedTerminal;
    int m_clientReportedMtts = 0;


private:
    void receiveGmcpModule(const GmcpModule &, bool);
    void resetGmcpModules();

public:
    void onAnalyzeUserStream(const TelnetIacBytes &);

public:
    void onSendToUser(const QString &data, bool goAhead);
    void onConnected();
    void reset() override;
    void onRelayEchoMode(bool);
    void onGmcpToUser(const GmcpMessage &);
    void onSendMSSPToUser(const TelnetMsspBytes &);
};
