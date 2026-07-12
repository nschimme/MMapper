// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "ClientTelnetBackend.h"

struct NODISCARD ClientTelnetBackend::Outputs final : public ClientTelnetOutputs
{
private:
    ClientController &m_controller;

public:
    explicit Outputs(ClientController &controller)
        : m_controller{controller}
    {}

private:
    void virt_connected() final { m_controller.onConnected(); }
    void virt_disconnected() final { m_controller.onDisconnected(); }
    void virt_socketError(const QString &msg) final { m_controller.onSocketError(msg); }
    void virt_echoModeChanged(const bool echo) final { m_controller.onEchoModeChanged(echo); }
    void virt_sendToUser(const QString &data) final { m_controller.onSendToUser(data); }
};

ClientTelnetBackend::ClientTelnetBackend(ConnectionListener &listener, ClientController &controller)
    : m_listener{listener}
    , m_controller{controller}
    , m_outputs{std::make_unique<Outputs>(controller)}
    , m_telnet{std::make_unique<ClientTelnet>(*m_outputs)}
{}

ClientTelnetBackend::~ClientTelnetBackend()
{
    m_telnet.reset();
    m_outputs.reset();
}

void ClientTelnetBackend::virt_connectToMud()
{
    m_telnet->connectToHost(m_listener);
}

void ClientTelnetBackend::virt_disconnectFromMud()
{
    m_telnet->disconnectFromHost();
}

bool ClientTelnetBackend::virt_isConnected() const
{
    return m_telnet->isConnected();
}

void ClientTelnetBackend::virt_sendToMud(const QString &data)
{
    m_telnet->sendToMud(data);
}

void ClientTelnetBackend::virt_windowSizeChanged(const int width, const int height)
{
    m_telnet->onWindowSizeChanged(width, height);
}
