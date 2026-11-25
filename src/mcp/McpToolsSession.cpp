// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../configuration/configuration.h"
#include "../proxy/connectionlistener.h"
#include "McpBackend.h"
#include "McpSession.h"
#include "McpToolRegistry.h"
#include "McpTools.h"

#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>

namespace {

NODISCARD bool isLoopbackHost(const QString &host)
{
    QHostAddress address;
    if (address.setAddress(host)) {
        return address.isLoopback();
    }
    return host.compare(QStringLiteral("localhost"), Qt::CaseInsensitive) == 0;
}

NODISCARD McpToolResult describe(const McpSession &session, const bool created)
{
    QJsonObject result;
    result[QStringLiteral("owner")] = QString::fromLatin1(mcpOwnerName(session.owner()));
    result[QStringLiteral("created")] = created;
    return McpToolResult::ok(QString::fromUtf8(QJsonDocument{result}.toJson(QJsonDocument::Compact)),
                             result);
}

} // namespace

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
                         const auto &connection = getConfig().connection;
                         const bool online = getConfig().general.mapMode != MapModeEnum::OFFLINE;
                         if (session.owner() == McpSession::OwnerEnum::None && online
                             && !isLoopbackHost(connection.remoteServerName)) {
                             return McpToolResult::error(
                                 QStringLiteral(
                                     "refusing to start a session: map mode is not OFFLINE and it "
                                     "would connect to %1:%2. Use OFFLINE mode, or point the MUD "
                                     "host at this computer, in the settings file. (get_status "
                                     "shows what is configured.)")
                                     .arg(connection.remoteServerName)
                                     .arg(connection.remotePort));
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
