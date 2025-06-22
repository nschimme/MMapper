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
    void onRelayTermTypeFromUserToMud(const TelnetTermTypeBytes &bytes) // Client sends its TTYPE/MTTS string
    {
        virt_onRelayTermTypeFromUserToMud(bytes);
    }

    // MNES and MTTS relay methods
    void onRelayMnesSubnegotiationToMud(const RawBytes &data)
    {
        virt_onRelayMnesSubnegotiationToMud(data);
    }
    void onRelayMnesStateToMud(bool client_will_mnes, bool client_do_mnes)
    {
        virt_onRelayMnesStateToMud(client_will_mnes, client_do_mnes);
    }
    void onRelayMttsReportToMud(const TelnetTermTypeBytes &report_data) // Client sends its MTTS report specifically
    {
        virt_onRelayMttsReportToMud(report_data);
    }
    void onRelayTerminalTypeSendRequestToMud() // Client requests MUD's TTYPE sequence
    {
        virt_onRelayTerminalTypeSendRequestToMud();
    }

private:
    virtual void virt_onAnalyzeUserStream(const RawBytes &, bool) = 0;
    virtual void virt_onSendToSocket(const TelnetIacBytes &) = 0;
    virtual void virt_onRelayGmcpFromUserToMud(const GmcpMessage &) = 0;
    virtual void virt_onRelayNawsFromUserToMud(int, int) = 0;
    virtual void virt_onRelayTermTypeFromUserToMud(const TelnetTermTypeBytes &) = 0; // Client TTYPE IS ...

    // MNES and MTTS relay virtual methods
    virtual void virt_onRelayMnesSubnegotiationToMud(const RawBytes &data) = 0;
    virtual void virt_onRelayMnesStateToMud(bool client_will_mnes, bool client_do_mnes) = 0;
    virtual void virt_onRelayMttsReportToMud(const TelnetTermTypeBytes &report_data) = 0;
    virtual void virt_onRelayTerminalTypeSendRequestToMud() = 0;
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
    int m_ttypeSendCount = 0; // For TTYPE SEND sequence with client
    TelnetTermTypeBytes m_clientReportedTermType; // Stores what the client sent as its primary TTYPE
    bool m_clientSupportsMnes = false; // Tracks if the connected client supports MNES

public:
    explicit UserTelnet(UserTelnetOutputs &outputs);
    ~UserTelnet() final = default;

private:
    NODISCARD bool virt_isGmcpModuleEnabled(const GmcpModuleTypeEnum &name) const final;
    void virt_sendToMapper(const RawBytes &data, bool goAhead) final;
    void virt_receiveGmcpMessage(const GmcpMessage &) final;
    void virt_receiveTerminalType(const TelnetTermTypeBytes &) final; // Client has sent TTYPE IS ...
    void virt_receiveWindowSize(int, int) final;
    void virt_sendRawData(const TelnetIacBytes &data) final;

    // MNES and MTTS handling
    void virt_receiveMnesSubnegotiation(const AppendBuffer &subnegotiation_data) final;
    void virt_mnesStateChanged(bool us_will_mnes, bool them_will_mnes) final; // us_will_mnes is client's WILL, them_will_mnes is client's DO
    void virt_receiveMttsReport(const TelnetTermTypeBytes &report_data) final; // Client sent specific MTTS report
    void virt_receivedTerminalTypeSendRequest() final; // Client sent TTYPE SEND

private:
    void receiveGmcpModule(const GmcpModule &, bool);
    void resetGmcpModules();

public:
    void onAnalyzeUserStream(const TelnetIacBytes &);

public:
    void onSendToUser(const QString &data, bool goAhead);
    void onConnected();
    void onRelayEchoMode(bool);
    void onGmcpToUser(const GmcpMessage &);
    void onSendMSSPToUser(const TelnetMsspBytes &);

    // Called by Proxy from MudTelnet events
    void onMudRequestsDoMnes();
    void onMudRequestsWontMnes();
    void onMudRequestsWillMnes();
    void onMudRequestsDontMnes(); // Added
    void onMudSendsMnesSubnegotiation(const RawBytes &data);
    void onMudSendsTermTypeIs(const TelnetTermTypeBytes &termtype_data); // MUD sends its TTYPE/MTTS
    void onMudRequestsTerminalTypeSend(); // MUD wants our (client's) TTYPE sequence
};
