#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../global/macros.h"
#include "McpSession.h"

#include <QString>

class ConnectionListener;
class MapData;
class McpEventLog;
class McpToolRegistry;
class McpWaiters;

// The parts of MMapper the tools look at. References only; the owner keeps them alive.
struct NODISCARD McpHost final
{
    MapData &mapData;
    ConnectionListener &listener;
    QString settingsPath; // looked up once at start-up; see McpService
};

// Registers MMapper's tools in a registry. Each area of tools lives in its own file (see
// McpTools.h), so this stays a short list however many tools there are.
class NODISCARD McpBackend final
{
private:
    McpSession m_session;

public:
    McpBackend(const McpHost &host,
               McpEventLog &events,
               McpWaiters &waiters,
               McpToolRegistry &registry);
};
