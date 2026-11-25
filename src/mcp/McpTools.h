#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

// One function per area of tools. Each registers its tools in the registry; a tool is a small
// function that asks the application for something and formats the answer.

#include "../global/macros.h"
#include "McpToolRegistry.h"

#include <QString>

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

// read_events, wait_for_event
void mcpRegisterEventTools(McpToolRegistry &registry, McpEventLog &events, McpWaiters &waiters);

// open_session, close_session
void mcpRegisterSessionTools(McpToolRegistry &registry,
                             const McpHost &host,
                             McpSession &session,
                             McpEventLog &events,
                             McpWaiters &waiters);

// Types one line as the user would and answers with the events it caused, optionally after
// waiting for "prompt" or for text matching a regular expression (see the send tool). Shared by
// send and the per-command tools.
void mcpRunLine(const McpHost &host,
                McpEventLog &events,
                McpWaiters &waiters,
                const QString &line,
                const QString &waitFor,
                int timeoutMs,
                McpReply reply);

// Empty if MMapper may connect to its MUD host, otherwise why not. A test must never reach the
// live game by accident: outside OFFLINE mode only a MUD host on this computer is allowed.
NODISCARD QString mcpRefuseToConnect();

// send
void mcpRegisterDriveTools(McpToolRegistry &registry,
                           const McpHost &host,
                           McpEventLog &events,
                           McpWaiters &waiters);

// inject_mud, inject_gmcp
void mcpRegisterInjectTools(McpToolRegistry &registry, const McpHost &host);

// load_map, save_map
void mcpRegisterMapTools(McpToolRegistry &registry, const McpHost &host, McpWaiters &waiters);

// One tool per MMapper command (the ones typed with the command prefix, e.g. _search), built
// from the command table in parser/AbstractParser-Commands.h
void mcpRegisterCommandTools(McpToolRegistry &registry,
                             const McpHost &host,
                             McpEventLog &events,
                             McpWaiters &waiters);

// get_room
void mcpRegisterRoomTools(McpToolRegistry &registry, const McpHost &host);

// map_summary, list_rooms, dump_map
void mcpRegisterMapInspectionTools(McpToolRegistry &registry, const McpHost &host);
