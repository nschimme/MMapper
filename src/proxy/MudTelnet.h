#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "../global/Signal2.h"
#include "../mpi/remoteeditsession.h"
#include "AbstractTelnet.h"

#include <QByteArray>
#include <QObject>

struct NODISCARD MsspTime final
{
    int year = -1;
    int month = -1;
    int day = -1;
    int hour = -1;
};

struct NODISCARD MudTelnetOutputs
{
    virtual ~MudTelnetOutputs();

public:
    void onAnalyzeMudStream(const RawBytes &bytes, const bool goAhead)
    {
        virt_onAnalyzeMudStream(bytes, goAhead);
    }
    void onSendToSocket(const TelnetIacBytes &bytes) { virt_onSendToSocket(bytes); }
    void onRelayEchoMode(const bool echo) { virt_onRelayEchoMode(echo); }
    void onRelayGmcpFromMudToUser(const GmcpMessage &msg) { virt_onRelayGmcpFromMudToUser(msg); }
    void onSendMSSPToUser(const TelnetMsspBytes &bytes) { virt_onSendMSSPToUser(bytes); }
    void onSendGameTimeToClock(const MsspTime &time) { virt_onSendGameTimeToClock(time); }
    void onTryCharLogin() { virt_onTryCharLogin(); }

    // MNES and MTTS relay methods
    void onRelayMnesSubnegotiationToUser(const RawBytes &data)
    {
        virt_onRelayMnesSubnegotiationToUser(data);
    }
    void onRelayMnesStateToUser(bool mud_will_mnes, bool mud_do_mnes) // Renamed for clarity
    {
        virt_onRelayMnesStateToUser(mud_will_mnes, mud_do_mnes);
    }
    void onRelayMttsReportToUser(const TelnetTermTypeBytes &report_data)
    {
        virt_onRelayMttsReportToUser(report_data);
    }
    void onRelayTerminalTypeSendRequestToUser() // MUD requests TTYPE sequence
    {
        virt_onRelayTerminalTypeSendRequestToUser();
    }
    void onMumeClientView(const QString &title, const QString &body)
    {
        virt_onMumeClientView(title, body);
    }
    void onMumeClientEdit(const RemoteSessionId id, const QString &title, const QString &body)
    {
        virt_onMumeClientEdit(id, title, body);
    }
    void onMumeClientError(const QString &errmsg) { virt_onMumeClientError(errmsg); }

private:
    virtual void virt_onAnalyzeMudStream(const RawBytes &, bool goAhead) = 0;
    virtual void virt_onSendToSocket(const TelnetIacBytes &) = 0;
    virtual void virt_onRelayEchoMode(bool) = 0;
    virtual void virt_onRelayGmcpFromMudToUser(const GmcpMessage &) = 0;
    virtual void virt_onSendMSSPToUser(const TelnetMsspBytes &) = 0;
    virtual void virt_onSendGameTimeToClock(const MsspTime &) = 0;
    virtual void virt_onTryCharLogin() = 0;

    // MNES and MTTS relay virtual methods
    virtual void virt_onRelayMnesSubnegotiationToUser(const RawBytes &data) = 0;
    virtual void virt_onRelayMnesStateToUser(bool mud_will_mnes, bool mud_do_mnes) = 0;
    virtual void virt_onRelayMttsReportToUser(const TelnetTermTypeBytes &report_data) = 0;
    virtual void virt_onRelayTerminalTypeSendRequestToUser() = 0;

    virtual void virt_onMumeClientView(const QString &title, const QString &body) = 0;
    virtual void virt_onMumeClientEdit(const RemoteSessionId id,
                                       const QString &title,
                                       const QString &body)
        = 0;
    virtual void virt_onMumeClientError(const QString &errmsg) = 0;
};

class NODISCARD MudTelnet final : public AbstractTelnet
{
private:
    MudTelnetOutputs &m_outputs;
    /** modules for GMCP */
    GmcpModuleSet m_gmcp;
    QString m_lineBuffer;
    bool m_receivedExternalDiscordHello = false;

public:
    explicit MudTelnet(MudTelnetOutputs &outputs);
    ~MudTelnet() final = default;

private:
    void virt_sendToMapper(const RawBytes &data, bool goAhead) final;
    void virt_receiveEchoMode(bool toggle) final;
    void virt_receiveGmcpMessage(const GmcpMessage &) final;
    void virt_receiveMudServerStatus(const TelnetMsspBytes &) final;
    void virt_onGmcpEnabled() final;
    void virt_sendRawData(const TelnetIacBytes &data) final;

    // MNES and MTTS handling
    void virt_receiveMnesSubnegotiation(const AppendBuffer &subnegotiation_data) final;
    void virt_mnesStateChanged(bool us_will_mnes, bool them_will_mnes) final; // us_will_mnes is MUD's WILL MNES, them_will_mnes is MUD's DO MNES
    void virt_receiveMttsReport(const TelnetTermTypeBytes &report_data) final;
    void virt_receivedTerminalTypeSendRequest() final;

private:
    void receiveGmcpModule(const GmcpModule &, bool);
    void resetGmcpModules();
    void parseMudServerStatus(const TelnetMsspBytes &);
    void submitOneLine(const QString &s);

private:
    void sendCoreSupports();

public:
    void onDisconnected();

public:
    void onAnalyzeMudStream(const TelnetIacBytes &);
    void onSubmitGmcpMumeClient(const GmcpMessage &);
    void onSendToMud(const QString &);
    void onRelayNaws(int, int);
    void onRelayTermType(const TelnetTermTypeBytes &); // From UserTelnet, for initial TTYPE or changes
    void onGmcpToMud(const GmcpMessage &);
    void onLoginCredentials(const QString &, const QString &);

    // Called by Proxy from UserTelnet events
    void onUserRequestsDoMnes();
    void onUserRequestsWontMnes();
    void onUserRequestsWillMnes();
    void onUserRequestsDontMnes(); // Added
    void onUserSendsMnesSubnegotiation(const RawBytes &data);
    void onUserSendsTermTypeIs(const TelnetTermTypeBytes &termtype_data); // For TTYPE IS, could be MTTS
    void onUserRequestsTerminalTypeSend(); // User wants MUD's TTYPE sequence
};
