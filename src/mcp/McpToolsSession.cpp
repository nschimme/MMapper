// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../configuration/configuration.h"
#include "../proxy/ProxyParserApi.h"
#include "../proxy/connectionlistener.h"
#include "../proxy/proxy.h"
#include "McpAccess.h"
#include "McpBackend.h"
#include "McpEventLog.h"
#include "McpSession.h"
#include "McpToolRegistry.h"
#include "McpToolSupport.h"
#include "McpTools.h"
#include "McpWaiters.h"

#include <algorithm>
#include <memory>

#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>

namespace {

NODISCARD McpToolResult describe(const McpSession &session, const bool created)
{
    QJsonObject result;
    result[QStringLiteral("owner")] = QString::fromLatin1(mcpOwnerName(session.owner()));
    result[QStringLiteral("created")] = created;
    return McpToolResult::json(result);
}

} // namespace

QString mcpRefuseToConnect()
{
    const auto &config = getConfig();
    return mcpConnectRefusal(config.general.mapMode == MapModeEnum::OFFLINE,
                             config.connection.remoteServerName,
                             config.connection.remotePort);
}

void mcpRegisterSessionTools(McpToolRegistry &registry,
                             const McpHost &host,
                             McpSession &session,
                             McpEventLog &events,
                             McpWaiters &waiters)
{
    registry.add(McpTool{
        QStringLiteral("open_session"),
        QStringLiteral(
            "Start a client session so there is a map, parser and MUD connection to work with, "
            "without needing a telnet client. If a session already exists (yours or a real "
            "client's) it is reported, not replaced. In PLAY or MAP mode this connects to the "
            "configured MUD host at once, so it is refused unless that host is this computer; "
            "OFFLINE mode never connects. Connecting to a plain telnet server can take 10 to 15 "
            "seconds, because MMapper tries TLS, then WebSocket, then plain TCP; pass "
            "wait_for_mud to wait until the connection is up."),
        mcpObjectSchema(
            {{QStringLiteral("wait_for_mud"),
              QJsonObject{{QStringLiteral("type"), QStringLiteral("boolean")},
                          {QStringLiteral("description"),
                           QStringLiteral("Wait until the MUD connection is up (not for OFFLINE "
                                          "mode, which never connects).")}}},
             {QStringLiteral("timeout_ms"),
              mcpIntegerProperty(QStringLiteral("How long to wait (default 30000, at most "
                                                "100000)."))}}),
        [&host, &session, &events, &waiters](const QJsonObject &args, McpReply reply) {
            const bool wait = args.value(QStringLiteral("wait_for_mud")).toBool(false);
            if (wait && getConfig().general.mapMode == MapModeEnum::OFFLINE) {
                reply(McpToolResult::error(QStringLiteral(
                    "OFFLINE mode never connects to a MUD; leave out wait_for_mud")));
                return;
            }
            if (session.owner() == McpSession::OwnerEnum::None) {
                if (const QString refusal = mcpRefuseToConnect(); !refusal.isEmpty()) {
                    reply(McpToolResult::error(refusal));
                    return;
                }
            }

            // Anything after this point counts as happening because of this call.
            const uint64_t since = events.lastSeq();
            const auto result = session.open();
            if (!result.error.isEmpty()) {
                reply(McpToolResult::error(result.error));
                return;
            }
            if (!wait) {
                reply(describe(session, result.created));
                return;
            }

            // Already connected, or connected while opening?
            Proxy *const proxy = host.listener.getProxy();
            const auto connectedNow = [&]() {
                return proxy != nullptr && ProxyMudConnectionApi{*proxy}.isConnected();
            };
            const auto seen = events.readSince(since, {QStringLiteral("mud_connected")});
            if (connectedNow() || !seen.events.empty()) {
                reply(describe(session, result.created));
                return;
            }
            if (proxy == nullptr) {
                reply(McpToolResult::error(QStringLiteral("the session closed")));
                return;
            }

            const int timeoutMs = std::clamp(args.value(QStringLiteral("timeout_ms")).toInt(30'000),
                                             100,
                                             MCP_MAX_WAIT_MS);
            const bool created = result.created;
            mcpWaitWhileSession(
                waiters,
                proxy,
                [](const McpEvent &e) { return e.kind == QStringLiteral("mud_connected"); },
                timeoutMs,
                [&session, created, reply](const McpWaiters::Outcome &outcome) {
                    switch (outcome.kind) {
                    case McpWaiters::Outcome::KindEnum::Matched:
                        reply(describe(session, created));
                        break;
                    case McpWaiters::Outcome::KindEnum::TimedOut:
                        reply(McpToolResult::error(
                            QStringLiteral("timed out waiting for the MUD connection; the session "
                                           "is still open and may yet connect (see get_status)")));
                        break;
                    case McpWaiters::Outcome::KindEnum::Cancelled:
                        reply(McpToolResult::error(outcome.reason));
                        break;
                    }
                });
        }});

    registry.addSync(QStringLiteral("close_session"),
                     QStringLiteral("End the session that open_session started. A session that "
                                    "belongs to a connected client is left alone."),
                     mcpObjectSchema(),
                     [&session](const QJsonObject &) {
                         const QString error = session.close();
                         if (!error.isEmpty()) {
                             return McpToolResult::error(error);
                         }
                         return describe(session, false);
                     });
}
