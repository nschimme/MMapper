// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "McpDispatcher.h"

#include <exception>
#include <memory>
#include <utility>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>
#include <QStringList>

namespace {

constexpr int ERR_PARSE = -32700;
constexpr int ERR_INVALID_REQUEST = -32600;
constexpr int ERR_METHOD_NOT_FOUND = -32601;
constexpr int ERR_INVALID_PARAMS = -32602;

NODISCARD QByteArray toJson(const QJsonObject &obj)
{
    return QJsonDocument{obj}.toJson(QJsonDocument::Compact);
}

NODISCARD QByteArray makeError(const QJsonValue &id, const int code, const QString &message)
{
    QJsonObject error;
    error[QStringLiteral("code")] = code;
    error[QStringLiteral("message")] = message;

    QJsonObject obj;
    obj[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
    obj[QStringLiteral("id")] = id; // null when the id could not be determined
    obj[QStringLiteral("error")] = error;
    return toJson(obj);
}

NODISCARD QByteArray makeResult(const QJsonValue &id, const QJsonObject &result)
{
    QJsonObject obj;
    obj[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
    obj[QStringLiteral("id")] = id;
    obj[QStringLiteral("result")] = result;
    return toJson(obj);
}

NODISCARD const QStringList &supportedVersions()
{
    static const QStringList versions{QStringLiteral("2025-11-25"),
                                      QStringLiteral("2025-06-18"),
                                      QStringLiteral("2025-03-26"),
                                      QStringLiteral("2024-11-05")};
    return versions;
}

NODISCARD QJsonObject toolToJson(const McpTool &tool)
{
    QJsonObject obj;
    obj[QStringLiteral("name")] = tool.name;
    obj[QStringLiteral("description")] = tool.description;
    obj[QStringLiteral("inputSchema")] = tool.inputSchema;
    return obj;
}

NODISCARD QJsonObject toolResultToJson(const McpToolResult &r)
{
    QJsonObject textItem;
    textItem[QStringLiteral("type")] = QStringLiteral("text");
    textItem[QStringLiteral("text")] = r.text;

    QJsonObject obj;
    QJsonArray content{textItem};
    if (r.png) {
        content.append(QJsonObject{{QStringLiteral("type"), QStringLiteral("image")},
                                   {QStringLiteral("data"), QString::fromLatin1(r.png->toBase64())},
                                   {QStringLiteral("mimeType"), QStringLiteral("image/png")}});
    }
    obj[QStringLiteral("content")] = content;
    obj[QStringLiteral("isError")] = r.isError;
    if (r.structured && r.structured->isObject()) {
        // structuredContent must be an object
        obj[QStringLiteral("structuredContent")] = *r.structured;
    }
    return obj;
}

} // namespace

McpDispatcher::McpDispatcher(const McpToolRegistry &registry,
                             QString serverName,
                             QString serverVersion,
                             QString instructions)
    : m_registry{registry}
    , m_serverName{std::move(serverName)}
    , m_serverVersion{std::move(serverVersion)}
    , m_instructions{std::move(instructions)}
{}

bool McpDispatcher::isSupportedProtocolVersion(const QString &version)
{
    return supportedVersions().contains(version);
}

void McpDispatcher::handle(const QByteArray &body, const Respond &respond) const
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        respond(makeError(QJsonValue::Null, ERR_PARSE, QStringLiteral("Parse error")));
        return;
    }
    if (!doc.isObject()) {
        // Includes batch arrays, which MCP does not allow.
        respond(makeError(QJsonValue::Null,
                          ERR_INVALID_REQUEST,
                          QStringLiteral("Expected a single JSON-RPC request object")));
        return;
    }

    const QJsonObject msg = doc.object();
    const bool hasId = msg.contains(QStringLiteral("id"));
    const QJsonValue id = msg.value(QStringLiteral("id"));
    const QString method = msg.value(QStringLiteral("method")).toString();

    if (msg.value(QStringLiteral("jsonrpc")).toString() != QStringLiteral("2.0")
        || (hasId && !(id.isString() || id.isDouble()))) {
        respond(makeError(QJsonValue::Null, ERR_INVALID_REQUEST, QStringLiteral("Invalid Request")));
        return;
    }

    // A message with no "method" is a response to something we never asked; ignore it.
    // A message with no id is a notification; it never gets a JSON-RPC reply.
    if (method.isEmpty() || !hasId) {
        if (hasId) {
            respond(makeError(id, ERR_INVALID_REQUEST, QStringLiteral("Missing method")));
        } else {
            respond(std::nullopt);
        }
        return;
    }

    const QJsonObject params = msg.value(QStringLiteral("params")).toObject();

    if (method == QStringLiteral("initialize")) {
        const QString requested = params.value(QStringLiteral("protocolVersion")).toString();
        // Echo the client's version if we speak it, otherwise offer our latest.
        const QString version = isSupportedProtocolVersion(requested)
                                    ? requested
                                    : QString::fromLatin1(LATEST_PROTOCOL_VERSION);
        QJsonObject serverInfo;
        serverInfo[QStringLiteral("name")] = m_serverName;
        serverInfo[QStringLiteral("version")] = m_serverVersion;

        QJsonObject capabilities;
        capabilities[QStringLiteral("tools")] = QJsonObject{{QStringLiteral("listChanged"), false}};

        QJsonObject result;
        result[QStringLiteral("protocolVersion")] = version;
        result[QStringLiteral("capabilities")] = capabilities;
        result[QStringLiteral("serverInfo")] = serverInfo;
        if (!m_instructions.isEmpty()) {
            result[QStringLiteral("instructions")] = m_instructions;
        }
        respond(makeResult(id, result));
        return;
    }

    if (method == QStringLiteral("ping")) {
        respond(makeResult(id, QJsonObject{}));
        return;
    }

    if (method == QStringLiteral("tools/list")) {
        QJsonArray tools;
        for (const McpTool &tool : m_registry.tools()) {
            tools.append(toolToJson(tool));
        }
        respond(makeResult(id, QJsonObject{{QStringLiteral("tools"), tools}}));
        return;
    }

    if (method == QStringLiteral("tools/call")) {
        const QString name = params.value(QStringLiteral("name")).toString();
        if (name.isEmpty()) {
            respond(makeError(id, ERR_INVALID_PARAMS, QStringLiteral("Missing tool name")));
            return;
        }
        const McpTool *tool = m_registry.find(name);
        if (tool == nullptr) {
            respond(makeError(id, ERR_INVALID_PARAMS, QStringLiteral("Unknown tool: ") + name));
            return;
        }
        const QJsonValue argsValue = params.value(QStringLiteral("arguments"));
        if (!argsValue.isUndefined() && !argsValue.isNull() && !argsValue.isObject()) {
            respond(makeError(id,
                              ERR_INVALID_PARAMS,
                              QStringLiteral("Tool arguments must be an object")));
            return;
        }

        // The reply may come later, so the closure owns everything it needs. It must also
        // be safe against a tool that (incorrectly) replies twice.
        auto answered = std::make_shared<bool>(false);
        McpReply reply = [respond, id, answered](McpToolResult result) {
            if (std::exchange(*answered, true)) {
                return;
            }
            respond(makeResult(id, toolResultToJson(result)));
        };

        try {
            tool->handler(argsValue.toObject(), reply);
        } catch (const std::exception &ex) {
            reply(McpToolResult::error(QStringLiteral("Tool failed: ")
                                       + QString::fromUtf8(ex.what())));
        } catch (...) {
            reply(McpToolResult::error(QStringLiteral("Tool failed with an unknown error")));
        }
        return;
    }

    respond(makeError(id, ERR_METHOD_NOT_FOUND, QStringLiteral("Method not found: ") + method));
}
