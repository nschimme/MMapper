// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "McpService.h"

#include "../configuration/configuration.h"
#include "../global/Version.h"
#include "../mapdata/mapdata.h"
#include "../observer/gameobserver.h"
#include "../pathmachine/mmapper2pathmachine.h"
#include "../proxy/GmcpMessage.h"

#include <QJsonObject>

McpService::McpService(const McpHost &host, GameObserver &observer)
    : QObject{nullptr}
    , m_host{host}
    , m_observer{observer}
    , m_waiters{this}
    , m_backend{m_host, m_events, m_waiters, m_registry}
    , m_dispatcher{m_registry,
                   QStringLiteral("MMapper"),
                   QString::fromLatin1(getMMapperVersion()),
                   QStringLiteral("Tools to inspect and drive a running MMapper: call get_status "
                                  "first to see its effective configuration.")}
    , m_server{m_dispatcher, this}
{
    connect(&m_server, &McpServer::sig_log, this, &McpService::sig_log);

    // Anything waiting for an event hears about each new one.
    m_events.setOnAppend([this](const McpEvent &event) { m_waiters.onEvent(event); });

    setConfig().mcp.registerChangeCallback(m_lifetime, [this]() { applyConfig(); });
    applyConfig();
}

McpService::~McpService()
{
    setCollecting(false);
    // Closing first drops connections that may be waiting for a reply from a tool.
    m_server.close();
    // Whatever was still waiting can no longer be answered; finish it before the objects it
    // refers to go away.
    m_waiters.cancelAll(QStringLiteral("MMapper is shutting down"));
    setConfig().mcp.setStatus(QStringLiteral("Stopped"));
}

void McpService::slot_onMapLoadFinished(const QString &fileName, const bool success)
{
    if (m_observing == nullptr) {
        return; // the server is off, so nobody can be waiting for it
    }
    QJsonObject data;
    data[QStringLiteral("file")] = fileName;
    data[QStringLiteral("success")] = success;
    std::ignore = m_events.append(QStringLiteral("map_load"), fileName, data);
}

void McpService::setCollecting(const bool collect)
{
    if (collect == (m_observing != nullptr)) {
        return;
    }
    if (!collect) {
        m_observing.reset(); // expires the Signal2 callbacks
        for (const auto &connection : m_qtConnections) {
            QObject::disconnect(connection);
        }
        m_qtConnections.clear();
        return;
    }

    m_observing = std::make_unique<Signal2Lifetime>();
    auto &lifetime = *m_observing;

    // Text the user is shown (the MUD's and MMapper's own), prompts, GMCP from the MUD, and what
    // was sent to the MUD.
    m_observer.sig2_sentToMudString.connect(lifetime, [this](const QString &text) {
        std::ignore = m_events.append(QStringLiteral("sent_to_mud"), text);
    });
    m_observer.sig2_sentToUserString.connect(lifetime, [this](const QString &text) {
        std::ignore = m_events.append(QStringLiteral("user_text"), text);
    });
    m_observer.sig2_sentToUserPrompt.connect(lifetime, [this](const QString &text) {
        std::ignore = m_events.append(QStringLiteral("prompt"), text);
    });
    m_observer.sig2_sentToUserGmcp.connect(lifetime, [this](const GmcpMessage &msg) {
        QJsonObject data;
        data[QStringLiteral("package")] = msg.getName().toQString();
        if (const auto &json = msg.getJson()) {
            data[QStringLiteral("json")] = json->toQString();
        }
        std::ignore = m_events.append(QStringLiteral("gmcp"), msg.getName().toQString(), data);
    });

    // Where the player is, and what the path machine and the map are doing about it.
    auto position = [this](const bool forced) {
        QJsonObject data;
        const auto id = m_host.mapData.getCurrentRoomId();
        data[QStringLiteral("roomId")] = id ? QJsonValue{static_cast<double>(id->asUint32())}
                                            : QJsonValue{QJsonValue::Null};
        data[QStringLiteral("forced")] = forced;
        std::ignore = m_events.append(QStringLiteral("position"), {}, data);
    };
    m_qtConnections.push_back(
        connect(&m_host.mapData, &MapData::sig_onPositionChange, this, [position]() {
            position(false);
        }));
    m_qtConnections.push_back(
        connect(&m_host.mapData, &MapData::sig_onForcedPositionChange, this, [position]() {
            position(true);
        }));
    m_qtConnections.push_back(connect(&m_host.mapData, &MapData::sig_onDataChanged, this, [this]() {
        std::ignore = m_events.append(QStringLiteral("map_changed"), {});
    }));
    m_qtConnections.push_back(
        connect(&m_host.pathMachine,
                &Mmapper2PathMachine::sig_state,
                this,
                [this](const QString &state) {
                    std::ignore = m_events.append(QStringLiteral("pathmachine"), state);
                }));
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
    setCollecting(false);
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

    setCollecting(true);
    m_appliedEnabled = enabled;
    m_appliedPort = port;
    m_appliedToken = token;
    m_hasApplied = true;
    settings.setStatus(QStringLiteral("Listening on 127.0.0.1:%1").arg(m_server.port()));
}
