// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "McpSession.h"

#include "../proxy/VirtualSocket.h"
#include "../proxy/connectionlistener.h"
#include "../proxy/proxy.h"

McpSession::McpSession(ConnectionListener &listener)
    : m_listener{listener}
{}

McpSession::~McpSession()
{
    std::ignore = close();
}

McpSession::OwnerEnum McpSession::owner() const
{
    Proxy *const proxy = m_listener.getProxy();
    if (proxy == nullptr) {
        return OwnerEnum::None;
    }
    if (proxy != m_ownedProxy.data()) {
        return OwnerEnum::Client;
    }
    return (m_socket != nullptr && m_socket->isConnected()) ? OwnerEnum::Mcp : OwnerEnum::Closing;
}

McpSession::OpenResult McpSession::open()
{
    if (m_listener.getProxy() != nullptr) {
        return OpenResult{{}, false}; // attach; never create a second session
    }

    // Same as ClientTelnet::connectToHost: our end, and the end the proxy will own.
    m_socket = std::make_unique<VirtualSocket>();
    auto peer = std::make_unique<VirtualSocket>();
    m_socket->connectToPeer(peer.get());
    QObject::connect(m_socket.get(), &QIODevice::readyRead, m_socket.get(), [this]() {
        std::ignore = m_socket->readAll();
    });
    m_listener.startClient(std::move(peer));

    m_ownedProxy = m_listener.getProxy();
    if (m_ownedProxy.isNull()) {
        m_socket.reset();
        return OpenResult{QStringLiteral("MMapper did not accept the session"), false};
    }
    return OpenResult{{}, true};
}

QString McpSession::close()
{
    switch (owner()) {
    case OwnerEnum::None:
    case OwnerEnum::Closing:
        m_socket.reset();
        return {};
    case OwnerEnum::Client:
        return QStringLiteral("the session belongs to a connected client, not to MCP");
    case OwnerEnum::Mcp:
        // The proxy notices the disconnect and tears itself down shortly after. Keep the socket
        // so owner() can tell "ours, closing" from "someone else's".
        m_socket->disconnectFromHost();
        return {};
    }
    return {};
}

const char *mcpOwnerName(const McpSession::OwnerEnum owner)
{
    switch (owner) {
    case McpSession::OwnerEnum::None:
        return "none";
    case McpSession::OwnerEnum::Mcp:
        return "mcp";
    case McpSession::OwnerEnum::Client:
        return "client";
    case McpSession::OwnerEnum::Closing:
        return "closing";
    }
    return "none";
}
