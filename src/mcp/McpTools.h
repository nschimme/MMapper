#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

// One function per area of tools. Each registers its tools in the registry; a tool is a small
// function that asks the application for something and formats the answer.

struct McpHost;
class McpEventLog;
class McpSession;
class McpToolRegistry;
class McpWaiters;

// get_status
void mcpRegisterStatusTools(McpToolRegistry &registry,
                            const McpHost &host,
                            McpEventLog &events,
                            McpSession &session);

// read_events
void mcpRegisterEventTools(McpToolRegistry &registry, McpEventLog &events);

// open_session, close_session
void mcpRegisterSessionTools(McpToolRegistry &registry, const McpHost &host, McpSession &session);

// send
void mcpRegisterDriveTools(McpToolRegistry &registry,
                           const McpHost &host,
                           McpEventLog &events,
                           McpWaiters &waiters);
