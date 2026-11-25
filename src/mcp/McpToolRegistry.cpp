// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "McpToolRegistry.h"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QStringList>

namespace {

// Not QString::toStdString(), which MMapper bans because it silently assumes UTF-8.
NODISCARD std::string utf8(const QString &s)
{
    const QByteArray bytes = s.toUtf8();
    return std::string{bytes.constData(), static_cast<size_t>(bytes.size())};
}

NODISCARD bool isValidToolName(const QString &name)
{
    // MCP: 1-128 characters from [A-Za-z0-9_.-]
    static const QRegularExpression re{QStringLiteral("^[A-Za-z0-9_.-]{1,128}$")};
    return re.match(name).hasMatch();
}
} // namespace

void McpToolRegistry::add(McpTool tool)
{
    if (!isValidToolName(tool.name)) {
        throw std::invalid_argument("invalid MCP tool name: " + utf8(tool.name));
    }
    if (tool.inputSchema.value(QStringLiteral("type")).toString() != QStringLiteral("object")) {
        throw std::invalid_argument("MCP tool schema must be an object: " + utf8(tool.name));
    }
    if (!tool.handler) {
        throw std::invalid_argument("MCP tool has no handler: " + utf8(tool.name));
    }
    // The description is all a model has to go on when choosing a tool.
    if (tool.description.trimmed().isEmpty()) {
        throw std::invalid_argument("MCP tool has no description: " + utf8(tool.name));
    }

    const auto it = std::find_if(m_tools.begin(), m_tools.end(), [&tool](const McpTool &t) {
        return t.name == tool.name;
    });
    if (it != m_tools.end()) {
        *it = std::move(tool);
    } else {
        m_tools.push_back(std::move(tool));
    }
}

void McpToolRegistry::addSync(QString name,
                              QString description,
                              QJsonObject inputSchema,
                              std::function<McpToolResult(const QJsonObject &)> fn)
{
    add(McpTool{std::move(name),
                std::move(description),
                std::move(inputSchema),
                [handler = std::move(fn)](const QJsonObject &args, McpReply reply) {
                    reply(handler(args));
                }});
}

const McpTool *McpToolRegistry::find(const QString &name) const
{
    const auto it = std::find_if(m_tools.begin(), m_tools.end(), [&name](const McpTool &t) {
        return t.name == name;
    });
    return it == m_tools.end() ? nullptr : &*it;
}

QString mcpCompactJson(const QJsonObject &object)
{
    return QString::fromUtf8(QJsonDocument{object}.toJson(QJsonDocument::Compact));
}

McpToolResult McpToolResult::json(const QJsonObject &object)
{
    return ok(mcpCompactJson(object), object);
}

QJsonObject mcpObjectSchema(const QJsonObject &properties, const QStringList &required)
{
    QJsonObject schema;
    schema[QStringLiteral("type")] = QStringLiteral("object");
    schema[QStringLiteral("properties")] = properties;
    if (!required.isEmpty()) {
        schema[QStringLiteral("required")] = QJsonArray::fromStringList(required);
    }
    return schema;
}

QJsonObject mcpStringProperty(const QString &description)
{
    return QJsonObject{{QStringLiteral("type"), QStringLiteral("string")},
                       {QStringLiteral("description"), description}};
}

QJsonObject mcpIntegerProperty(const QString &description)
{
    return QJsonObject{{QStringLiteral("type"), QStringLiteral("integer")},
                       {QStringLiteral("description"), description}};
}
