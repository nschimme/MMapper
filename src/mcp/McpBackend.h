#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../global/macros.h"

class ConnectionListener;
class MapData;
class McpEventLog;
class McpToolRegistry;

// The parts of MMapper the tools look at. References only; the owner keeps them alive.
struct NODISCARD McpHost final
{
    MapData &mapData;
    ConnectionListener &listener;
};

// Registers MMapper's tools (state, events, ...) in a registry. This is the only place that
// knows about both the MCP layer and the rest of MMapper; each tool is a small function that
// asks the application for something and formats the answer.
class NODISCARD McpBackend final
{
public:
    McpBackend(const McpHost &host, McpEventLog &events, McpToolRegistry &registry);
};
