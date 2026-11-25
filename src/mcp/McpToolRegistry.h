#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../global/macros.h"

#include <functional>
#include <optional>
#include <vector>

#include <QJsonObject>
#include <QJsonValue>
#include <QString>

// What a tool hands back. A tool that ran but failed (bad arguments, no session, ...) sets
// isError; that is reported to the model as a normal result so it can read the message and
// retry. Protocol-level problems are the dispatcher's business, not a tool's.
struct NODISCARD McpToolResult final
{
    bool isError = false;
    QString text;
    std::optional<QJsonValue> structured;

    NODISCARD static McpToolResult ok(QString text, std::optional<QJsonValue> structured = {})
    {
        return McpToolResult{false, std::move(text), std::move(structured)};
    }
    NODISCARD static McpToolResult error(QString text)
    {
        return McpToolResult{true, std::move(text), std::nullopt};
    }
    // A result that is a JSON object: its compact text for a reader of the text, and the object
    // itself as the structured content.
    NODISCARD static McpToolResult json(const QJsonObject &object);
};

NODISCARD QString mcpCompactJson(const QJsonObject &object);

// A tool answers by calling this exactly once, now or later (e.g. when an event arrives or a
// timer fires). Replying later is how "wait for X" tools avoid nested event loops.
using McpReply = std::function<void(McpToolResult)>;
using McpToolHandler = std::function<void(const QJsonObject &arguments, McpReply reply)>;

struct NODISCARD McpTool final
{
    QString name;
    QString description;
    QJsonObject inputSchema; // JSON Schema object; must have "type": "object"
    McpToolHandler handler;
};

class NODISCARD McpToolRegistry final
{
private:
    std::vector<McpTool> m_tools;

public:
    // Replaces an existing tool with the same name (so a hand-written tool can override a
    // generated one). Throws std::invalid_argument for a bad name, schema or handler.
    void add(McpTool tool);

    // Convenience for tools that answer immediately.
    void addSync(QString name,
                 QString description,
                 QJsonObject inputSchema,
                 std::function<McpToolResult(const QJsonObject &)> fn);

    NODISCARD const std::vector<McpTool> &tools() const { return m_tools; }
    NODISCARD const McpTool *find(const QString &name) const;
};

// {"type":"object","properties":{...},"required":[...]}; helpers keep schemas readable.
NODISCARD QJsonObject mcpObjectSchema(const QJsonObject &properties = {},
                                      const QStringList &required = {});
NODISCARD QJsonObject mcpStringProperty(const QString &description);
NODISCARD QJsonObject mcpIntegerProperty(const QString &description);
