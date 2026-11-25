// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "McpBackend.h"

#include "../configuration/configuration.h"
#include "../mapdata/mapdata.h"
#include "../proxy/ProxyParserApi.h"
#include "../proxy/connectionlistener.h"
#include "McpEventLog.h"
#include "McpToolRegistry.h"

#include <set>

#include <QJsonArray>
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

NODISCARD QString toCompactJson(const QJsonObject &obj)
{
    return QString::fromUtf8(QJsonDocument{obj}.toJson(QJsonDocument::Compact));
}

NODISCARD QJsonObject eventToJson(const McpEvent &e)
{
    QJsonObject obj;
    obj[QStringLiteral("seq")] = static_cast<double>(e.seq);
    obj[QStringLiteral("kind")] = e.kind;
    obj[QStringLiteral("text")] = e.text;
    if (!e.data.isEmpty()) {
        obj[QStringLiteral("data")] = e.data;
    }
    return obj;
}

} // namespace

McpBackend::McpBackend(const McpHost &host, McpEventLog &events, McpToolRegistry &registry)
{
    // What MMapper is *actually* set to do. A test harness checks this before it sends
    // anything: a settings file with a wrong section name is ignored without a word, which
    // once left an "offline" test run connected to the live game.
    registry.addSync(QStringLiteral("get_status"),
                     QStringLiteral(
                         "Report the effective MMapper configuration and session state: map mode "
                         "(PLAY, MAP or OFFLINE), the MUD host and port a session would connect "
                         "to, whether update checks are on, which settings file is in use, and "
                         "whether a client session is attached."),
                     mcpObjectSchema(),
                     [host, &events](const QJsonObject &) {
                         const auto &config = getConfig();

                         QJsonObject session;
                         const Proxy *const proxy = host.listener.getProxy();
                         session[QStringLiteral("attached")] = (proxy != nullptr);
                         if (proxy != nullptr) {
                             // The facade only needs a mutable reference to ask a read-only question.
                             session[QStringLiteral("mudConnected")]
                                 = ProxyMudConnectionApi{*const_cast<Proxy *>(proxy)}.isConnected();
                         }

                         QJsonObject status;
                         status[QStringLiteral("mapMode")] = mapModeName(config.general.mapMode);
                         status[QStringLiteral("remoteHost")] = config.connection.remoteServerName;
                         status[QStringLiteral("remotePort")] = static_cast<int>(
                             config.connection.remotePort);
                         status[QStringLiteral("checkForUpdate")] = config.general.checkForUpdate;
                         status[QStringLiteral("settingsPath")] = getSettingsFilePath();
                         status[QStringLiteral("session")] = session;
                         if (const auto id = host.mapData.getCurrentRoomId()) {
                             status[QStringLiteral("currentRoomId")] = static_cast<double>(
                                 id->asUint32());
                         } else {
                             status[QStringLiteral("currentRoomId")] = QJsonValue::Null;
                         }
                         status[QStringLiteral("lastEventSeq")] = static_cast<double>(
                             events.lastSeq());
                         return McpToolResult::ok(toCompactJson(status), status);
                     });

    registry.addSync(
        QStringLiteral("read_events"),
        QStringLiteral("Read what happened since a cursor: text from the MUD, text MMapper "
                       "showed the user, and GMCP messages. Pass the returned nextSeq as `since` "
                       "next time. `truncated` is true if older events were dropped before you "
                       "read them."),
        mcpObjectSchema(
            {{QStringLiteral("since"),
              mcpIntegerProperty(QStringLiteral("Return events after this sequence number "
                                                "(0 for everything still kept)."))},
             {QStringLiteral("kinds"),
              QJsonObject{{QStringLiteral("type"), QStringLiteral("array")},
                          {QStringLiteral("items"),
                           QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}},
                          {QStringLiteral("description"),
                           QStringLiteral("Only these kinds: mud_text, user_text, gmcp.")}}},
             {QStringLiteral("max"),
              mcpIntegerProperty(QStringLiteral("At most this many events (default 100, "
                                                "limit 1000)."))}}),
        [&events](const QJsonObject &args) {
            const double since = args.value(QStringLiteral("since")).toDouble(0);
            if (since < 0) {
                return McpToolResult::error(QStringLiteral("`since` must not be negative"));
            }
            const int maxArg = args.value(QStringLiteral("max")).toInt(100);
            const size_t max = static_cast<size_t>(std::clamp(maxArg, 1, 1000));

            std::set<QString> kinds;
            for (const QJsonValue &v : args.value(QStringLiteral("kinds")).toArray()) {
                kinds.insert(v.toString());
            }

            const auto read = events.readSince(static_cast<uint64_t>(since), kinds, max);
            QJsonArray list;
            for (const McpEvent &e : read.events) {
                list.append(eventToJson(e));
            }
            QJsonObject result;
            result[QStringLiteral("events")] = list;
            result[QStringLiteral("nextSeq")] = static_cast<double>(read.nextSeq);
            result[QStringLiteral("truncated")] = read.truncated;
            return McpToolResult::ok(toCompactJson(result), result);
        });
}
