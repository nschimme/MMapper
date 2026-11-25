// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../configuration/configuration.h"
#include "../proxy/connectionlistener.h"
#include "McpAccess.h"
#include "McpBackend.h"
#include "McpSession.h"
#include "McpToolRegistry.h"
#include "McpTools.h"

#include <QJsonDocument>
#include <QJsonObject>

namespace {

NODISCARD McpToolResult describe(const McpSession &session, const bool created)
{
    QJsonObject result;
    result[QStringLiteral("owner")] = QString::fromLatin1(mcpOwnerName(session.owner()));
    result[QStringLiteral("created")] = created;
    return McpToolResult::ok(QString::fromUtf8(QJsonDocument{result}.toJson(QJsonDocument::Compact)),
                             result);
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
                             const McpHost & /*host*/,
                             McpSession &session)
{
    registry.addSync(QStringLiteral("open_session"),
                     QStringLiteral(
                         "Start a client session so there is a map, parser and MUD connection to "
                         "work with, without needing a telnet client. If a session already exists "
                         "(yours or a real client's) it is reported, not replaced. In PLAY or MAP "
                         "mode this connects to the configured MUD host at once, so it is refused "
                         "unless that host is this computer; OFFLINE mode never connects."),
                     mcpObjectSchema(),
                     [&session](const QJsonObject &) {
                         if (session.owner() == McpSession::OwnerEnum::None) {
                             if (const QString refusal = mcpRefuseToConnect(); !refusal.isEmpty()) {
                                 return McpToolResult::error(refusal);
                             }
                         }
                         const auto result = session.open();
                         if (!result.error.isEmpty()) {
                             return McpToolResult::error(result.error);
                         }
                         return describe(session, result.created);
                     });

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
