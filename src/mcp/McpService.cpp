// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "McpService.h"

#include "../configuration/configuration.h"
#include "../global/Version.h"
#include "../observer/gameobserver.h"
#include "../proxy/GmcpMessage.h"

#include <QJsonObject>

McpService::McpService(const McpHost &host, GameObserver &observer)
    : QObject{nullptr}
    , m_host{host}
    , m_backend{m_host, m_events, m_registry}
    , m_dispatcher{m_registry,
                   QStringLiteral("MMapper"),
                   QString::fromLatin1(getMMapperVersion()),
                   QStringLiteral("Tools to inspect and drive a running MMapper: call get_status "
                                  "first to see its effective configuration.")}
    , m_server{m_dispatcher, this}
{
    connect(&m_server, &McpServer::sig_log, this, &McpService::sig_log);

    // Everything that happens goes into the event log. Nothing here costs anything while the
    // service does not exist, i.e. when the MCP feature is compiled out.
    observer.sig2_sentToMudString.connect(m_lifetime, [this](const QString &text) {
        std::ignore = m_events.append(QStringLiteral("mud_text"), text);
    });
    observer.sig2_sentToUserString.connect(m_lifetime, [this](const QString &text) {
        std::ignore = m_events.append(QStringLiteral("user_text"), text);
    });
    observer.sig2_sentToUserGmcp.connect(m_lifetime, [this](const GmcpMessage &msg) {
        QJsonObject data;
        data[QStringLiteral("package")] = msg.getName().toQString();
        if (const auto &json = msg.getJson()) {
            data[QStringLiteral("json")] = json->toQString();
        }
        std::ignore = m_events.append(QStringLiteral("gmcp"), msg.getName().toQString(), data);
    });

    setConfig().mcp.registerChangeCallback(m_lifetime, [this]() { applyConfig(); });
    applyConfig();
}

McpService::~McpService()
{
    // Closing first drops connections that may be waiting for a reply from a tool.
    m_server.close();
    setConfig().mcp.setStatus(QStringLiteral("Stopped"));
}

void McpService::applyConfig()
{
    auto &settings = setConfig().mcp;
    const bool enabled = settings.isEnabled();
    const uint16_t port = settings.getPort();
    const QString token = settings.getToken();

    if (m_hasApplied && enabled == m_appliedEnabled && port == m_appliedPort
        && token == m_appliedToken) {
        return;
    }

    m_server.close();
    if (!enabled) {
        m_appliedEnabled = false;
        m_hasApplied = true;
        m_appliedPort = port;
        m_appliedToken = token;
        settings.setStatus(QStringLiteral("Disabled"));
        return;
    }

    const QString error = m_server.listen(port, token);
    if (!error.isEmpty()) {
        // Not fatal, and not remembered as applied, so changing a setting tries again.
        m_hasApplied = false;
        settings.setStatus(QStringLiteral("Could not listen on port %1: %2").arg(port).arg(error));
        emit sig_log("MCP", settings.getStatus());
        return;
    }

    m_appliedEnabled = enabled;
    m_appliedPort = port;
    m_appliedToken = token;
    m_hasApplied = true;
    settings.setStatus(QStringLiteral("Listening on 127.0.0.1:%1").arg(m_server.port()));
}
