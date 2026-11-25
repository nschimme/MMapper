// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../proxy/GmcpMessage.h"
#include "../proxy/ProxyParserApi.h"
#include "../proxy/connectionlistener.h"
#include "../proxy/proxy.h"
#include "McpBackend.h"
#include "McpToolRegistry.h"
#include "McpTools.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

void mcpRegisterInjectTools(McpToolRegistry &registry, const McpHost &host)
{
    registry.addSync(
        QStringLiteral("inject_mud"),
        QStringLiteral("Feed text to MMapper as if the MUD had sent it, without a MUD. For "
                       "MUME this is the XML-tagged stream (<room><name>..</name>..</room>, "
                       "<exits>..</exits>, <prompt>..</prompt>), so a recorded session can be "
                       "replayed. Complete lines must end with a newline; a prompt has no "
                       "newline and sets prompt=true. The stream must be well-formed MUME XML, as "
                       "recorded from the game (for instance the newline after the exits "
                       "belongs inside the <exits> tag); debug builds of MMapper abort on a "
                       "stream that breaks the parser's assumptions. Needs a session (see "
                       "open_session). "
                       "Whether this changes the map depends on the map mode: OFFLINE never "
                       "edits the map."),
        mcpObjectSchema({{QStringLiteral("text"),
                          mcpStringProperty(QStringLiteral("The bytes the MUD would send (UTF-8)."))},
                         {QStringLiteral("prompt"),
                          QJsonObject{{QStringLiteral("type"), QStringLiteral("boolean")},
                                      {QStringLiteral("description"),
                                       QStringLiteral("True if this text ends with a prompt (the MUD "
                                                      "would follow it with a go-ahead).")}}}},
                        {QStringLiteral("text")}),
        [&host](const QJsonObject &args) {
            Proxy *const proxy = host.listener.getProxy();
            if (proxy == nullptr) {
                return McpToolResult::error(
                    QStringLiteral("there is no session; call open_session first"));
            }
            const QString text = args.value(QStringLiteral("text")).toString();
            if (text.isEmpty()) {
                return McpToolResult::error(QStringLiteral("`text` must not be empty"));
            }
            const bool prompt = args.value(QStringLiteral("prompt")).toBool(false);
            ProxyControlApi{*proxy}.injectMudBytes(text.toUtf8(), prompt);
            return McpToolResult::ok(QStringLiteral("injected %1 characters").arg(text.size()));
        });

    registry.addSync(
        QStringLiteral("inject_gmcp"),
        QStringLiteral("Feed a GMCP message to MMapper as if the MUD had sent it, for example "
                       "package \"Room.Info\" with the JSON the MUD would send. Needs a session "
                       "(see open_session)."),
        mcpObjectSchema({{QStringLiteral("package"),
                          mcpStringProperty(
                              QStringLiteral("The GMCP package name, e.g. Char.Vitals."))},
                         {QStringLiteral("json"),
                          QJsonObject{
                              {QStringLiteral("description"),
                               QStringLiteral("The payload: a JSON object or array, or a string "
                                              "holding JSON text. Omit for a message without a "
                                              "payload.")}}}},
                        {QStringLiteral("package")}),
        [&host](const QJsonObject &args) {
            Proxy *const proxy = host.listener.getProxy();
            if (proxy == nullptr) {
                return McpToolResult::error(
                    QStringLiteral("there is no session; call open_session first"));
            }
            const QString package = args.value(QStringLiteral("package")).toString().trimmed();
            if (package.isEmpty() || package.contains(QLatin1Char(' '))) {
                return McpToolResult::error(
                    QStringLiteral("`package` must be a name without spaces, e.g. Char.Vitals"));
            }

            QByteArray payload;
            const QJsonValue json = args.value(QStringLiteral("json"));
            if (json.isObject()) {
                payload = QJsonDocument{json.toObject()}.toJson(QJsonDocument::Compact);
            } else if (json.isArray()) {
                payload = QJsonDocument{json.toArray()}.toJson(QJsonDocument::Compact);
            } else if (json.isString()) {
                payload = json.toString().toUtf8();
            } else if (!json.isUndefined() && !json.isNull()) {
                return McpToolResult::error(
                    QStringLiteral("`json` must be an object, an array or a string"));
            }

            QByteArray raw = package.toUtf8();
            if (!payload.isEmpty()) {
                raw += ' ' + payload;
            }
            ProxyControlApi{*proxy}.injectMudGmcp(GmcpMessage::fromRawBytes(raw));
            return McpToolResult::ok(QStringLiteral("injected ") + package);
        });
}
