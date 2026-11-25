#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../global/macros.h"
#include "McpToolRegistry.h"

#include <functional>
#include <optional>

#include <QByteArray>
#include <QString>

// JSON-RPC 2.0 request handling for an MCP server, with no sockets and no HTTP.
//
// Takes the body of one POST and calls `respond` exactly once with the JSON body to send
// back, or with nullopt when nothing is sent back (a notification, which the HTTP layer
// answers with a bare 202). `respond` may be called after handle() returns, which is how a
// tool that waits for an event replies.
//
// Protocol notes (MCP, revisions 2024-11-05 through 2025-11-25):
// * "initialize" is answered, and "ping" returns {}; neither is required before tools/list or
//   tools/call, so stateless clients work too (no session is ever created).
// * JSON-RPC batch arrays are rejected; the transport spec allows a single message per POST.
// * A tool that fails returns a normal result with isError=true. Unknown tools and bad
//   parameters are JSON-RPC errors (-32602).
class NODISCARD McpDispatcher final
{
public:
    using Respond = std::function<void(std::optional<QByteArray>)>;

    static constexpr const char *LATEST_PROTOCOL_VERSION = "2025-11-25";

private:
    const McpToolRegistry &m_registry;
    QString m_serverName;
    QString m_serverVersion;
    QString m_instructions;

public:
    McpDispatcher(const McpToolRegistry &registry,
                  QString serverName,
                  QString serverVersion,
                  QString instructions = {});

    void handle(const QByteArray &body, const Respond &respond) const;

    NODISCARD static bool isSupportedProtocolVersion(const QString &version);
};
