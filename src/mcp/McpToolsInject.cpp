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

namespace {

struct NODISCARD GmcpArgs final
{
    QByteArray raw; // "Package {json}", as it goes over the wire
    QString package;
    QString error;
};

// Reads the `package` and `json` arguments shared by inject_gmcp and send_gmcp.
NODISCARD GmcpArgs parseGmcpArgs(const QJsonObject &args)
{
    GmcpArgs result;
    result.package = args.value(QStringLiteral("package")).toString().trimmed();
    if (result.package.isEmpty() || result.package.contains(QLatin1Char(' '))) {
        result.error = QStringLiteral("`package` must be a name without spaces, e.g. Char.Vitals");
        return result;
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
        result.error = QStringLiteral("`json` must be an object, an array or a string");
        return result;
    }

    result.raw = result.package.toUtf8();
    if (!payload.isEmpty()) {
        result.raw += ' ' + payload;
    }
    return result;
}

} // namespace

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

    registry.addSync(QStringLiteral("inject_gmcp"),
                     QStringLiteral(
                         "Feed a GMCP message to MMapper as if the MUD had sent it, for example "
                         "package \"Room.Info\" with the JSON the MUD would send. Needs a session "
                         "(see open_session)."),
                     mcpObjectSchema({{QStringLiteral("package"),
                                       mcpStringProperty(QStringLiteral(
                                           "The GMCP package name, e.g. Char.Vitals."))},
                                      {QStringLiteral("json"),
                                       QJsonObject{
                                           {QStringLiteral("description"),
                                            QStringLiteral(
                                                "The payload: a JSON object or array, or a string "
                                                "holding JSON text. Omit for a message without a "
                                                "payload.")}}}},
                                     {QStringLiteral("package")}),
                     [&host](const QJsonObject &args) {
                         Proxy *const proxy = host.listener.getProxy();
                         if (proxy == nullptr) {
                             return McpToolResult::error(
                                 QStringLiteral("there is no session; call open_session first"));
                         }
                         const GmcpArgs gmcp = parseGmcpArgs(args);
                         if (!gmcp.error.isEmpty()) {
                             return McpToolResult::error(gmcp.error);
                         }
                         ProxyControlApi{*proxy}.injectMudGmcp(GmcpMessage::fromRawBytes(gmcp.raw));
                         const QString package = gmcp.package;
                         return McpToolResult::ok(QStringLiteral("injected ") + package);
                     });

    registry.addSync(
        QStringLiteral("send_gmcp"),
        QStringLiteral("Send a GMCP message to the MUD, for example package \"Core.Hello\" with the "
                       "JSON a client would send. Needs a session connected to a MUD that has "
                       "agreed to use GMCP (MUME offers it right after connecting); otherwise this "
                       "is an error (MMapper would silently drop the message)."),
        mcpObjectSchema({{QStringLiteral("package"),
                          mcpStringProperty(
                              QStringLiteral("The GMCP package name, e.g. Core.Supports.Set."))},
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
            const GmcpArgs gmcp = parseGmcpArgs(args);
            if (!gmcp.error.isEmpty()) {
                return McpToolResult::error(gmcp.error);
            }
            if (!ProxyMudConnectionApi{*proxy}.isConnected()) {
                return McpToolResult::error(QStringLiteral("not connected to the MUD"));
            }
            if (!ProxyControlApi{*proxy}.isMudGmcpNegotiated()) {
                return McpToolResult::error(
                    QStringLiteral("the MUD has not agreed to use GMCP (yet), so the message "
                                   "would be dropped. MUME offers it as soon as it connects, so "
                                   "if the connection is brand new try again in a moment."));
            }
            ProxyMudGmcpApi{*proxy}.gmcpToMud(GmcpMessage::fromRawBytes(gmcp.raw));
            return McpToolResult::ok(QStringLiteral("sent ") + gmcp.package);
        });
}
