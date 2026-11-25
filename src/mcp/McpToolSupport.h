#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

// Small helpers the tools share, so that each tool is only what is particular to it.

#include "../global/macros.h"
#include "McpServer.h"
#include "McpWaiters.h"

#include <QString>

class Proxy;
struct McpHost;

// The longest a tool may wait for something. The server gives up on a reply after
// McpServer::Options::replyTimeoutMs, so a tool's own timeout has to be shorter, or the client
// gets a bare 504 instead of the tool's explanation.
inline constexpr int MCP_DEFAULT_WAIT_MS = 10'000;
inline constexpr int MCP_MAX_WAIT_MS = 100'000;
static_assert(MCP_MAX_WAIT_MS < McpServer::DEFAULT_REPLY_TIMEOUT_MS);

// What a tool says when it needs a session and there is none.
NODISCARD QString mcpNoSessionMessage();

// Waits for an event, like McpWaiters::add, but also ends the wait (as cancelled, "the session
// closed") if `session` is destroyed first. Pass nullptr when the wait does not belong to a
// session.
void mcpWaitWhileSession(McpWaiters &waiters,
                         Proxy *session,
                         McpWaiters::Predicate predicate,
                         int timeoutMs,
                         McpWaiters::Done done);
