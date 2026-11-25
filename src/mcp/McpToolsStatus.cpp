// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../configuration/configuration.h"
#include "../map/Map.h"
#include "../map/RoomHandle.h"
#include "../mapdata/mapdata.h"
#include "../proxy/ProxyParserApi.h"
#include "../proxy/connectionlistener.h"
#include "McpBackend.h"
#include "McpEventLog.h"
#include "McpSession.h"
#include "McpToolRegistry.h"
#include "McpTools.h"

#include <QJsonDocument>
#include <QJsonObject>

namespace {

NODISCARD QString mapModeName(const MapModeEnum mode)
{
    switch (mode) {
    case MapModeEnum::PLAY:
        return QStringLiteral("PLAY");
    case MapModeEnum::MAP:
        return QStringLiteral("MAP");
    case MapModeEnum::OFFLINE:
        return QStringLiteral("OFFLINE");
    }
    return QStringLiteral("UNKNOWN");
}

} // namespace

void mcpRegisterStatusTools(McpToolRegistry &registry,
                            const McpHost &host,
                            McpEventLog &events,
                            McpSession &session)
{
    // What MMapper is *actually* set to do. A test harness checks this before it sends
    // anything: a settings file with a wrong section name is ignored without a word, which
    // once left an "offline" test run connected to the live game.
    registry.addSync(
        QStringLiteral("get_status"),
        QStringLiteral("Report the effective MMapper configuration and session state: map mode "
                       "(PLAY, MAP or OFFLINE), the MUD host and port a session would connect "
                       "to, whether update checks are on, which settings file is in use, and "
                       "the session: whether one exists, who owns it (mcp or a real client) "
                       "and whether it is connected to the MUD."),
        mcpObjectSchema(),
        [&host, &events, &session](const QJsonObject &) {
            const auto &config = getConfig();

            QJsonObject sessionJson;
            Proxy *const proxy = host.listener.getProxy();
            sessionJson[QStringLiteral("attached")] = (proxy != nullptr);
            sessionJson[QStringLiteral("owner")] = QString::fromLatin1(
                mcpOwnerName(session.owner()));
            if (proxy != nullptr) {
                sessionJson[QStringLiteral("mudConnected")] = ProxyMudConnectionApi{*proxy}
                                                                  .isConnected();
            }

            QJsonObject status;
            status[QStringLiteral("mapMode")] = mapModeName(config.general.mapMode);
            status[QStringLiteral("remoteHost")] = config.connection.remoteServerName;
            status[QStringLiteral("remotePort")] = static_cast<int>(config.connection.remotePort);
            status[QStringLiteral("checkForUpdate")] = config.general.checkForUpdate;
            status[QStringLiteral("settingsPath")] = host.settingsPath;
            status[QStringLiteral("session")] = sessionJson;
            // currentRoomId is MMapper's internal id; the external id is the one
            // get_room and list_rooms use. The server id is the MUD's own, and is
            // only there when the room has one.
            status[QStringLiteral("currentRoomId")] = QJsonValue::Null;
            status[QStringLiteral("currentRoomExternalId")] = QJsonValue::Null;
            if (const auto id = host.mapData.getCurrentRoomId()) {
                status[QStringLiteral("currentRoomId")] = static_cast<double>(id->asUint32());
                const Map map = host.mapData.getCurrentMap();
                if (const RoomHandle room = map.findRoomHandle(*id); room.exists()) {
                    status[QStringLiteral("currentRoomExternalId")] = static_cast<double>(
                        room.getIdExternal().asUint32());
                    if (const auto server = room.getServerId(); server != INVALID_SERVER_ROOMID) {
                        status[QStringLiteral("currentRoomServerId")] = static_cast<double>(
                            server.asUint32());
                    }
                }
            }
            status[QStringLiteral("lastEventSeq")] = static_cast<double>(events.lastSeq());
            return McpToolResult::ok(QString::fromUtf8(
                                         QJsonDocument{status}.toJson(QJsonDocument::Compact)),
                                     status);
        });
}
