#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../global/macros.h"
#include "McpHttp.h"

#include <cstdint>
#include <optional>

#include <QByteArray>
#include <QString>

// Who may talk to the server, decided from the request alone (no sockets), so it can be tested.
//
// The server listens on loopback only, but a web page in the user's browser can still send
// requests to loopback, and DNS rebinding can make such a request look same-origin. So every
// request needs the bearer token, and the Host and Origin headers must name this machine.
struct NODISCARD McpAccessPolicy final
{
    QByteArray token;  // bearer token; an empty token rejects everything
    uint16_t port = 0; // the port actually being listened on
};

// Constant-time comparison, so response timing does not reveal how much of a guess matched.
NODISCARD bool mcpTokensEqual(const QByteArray &expected, const QByteArray &given);

// Returns nullopt if the request may go on to the JSON-RPC layer, otherwise the error to send.
// The endpoint is POST /mcp; anything else is a transport-level error, never a JSON-RPC one.
NODISCARD std::optional<McpHttpError> mcpCheckRequest(const McpHttpRequest &request,
                                                      const McpAccessPolicy &policy);

// Whether MMapper may connect to its MUD host. A test must never reach the live game by
// accident: outside OFFLINE mode only a MUD host on this computer is allowed. Returns an
// empty string if connecting is fine, otherwise why not.
NODISCARD QString mcpConnectRefusal(bool offlineMode, const QString &host, uint16_t port);

// True for the MMapper command that connects to the MUD: the command prefix followed by
// "connect", or by an abbreviation of it down to four letters ("_conn").
NODISCARD bool mcpIsConnectCommand(const QString &line, char prefix);
