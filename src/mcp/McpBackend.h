#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../global/macros.h"
#include "../mapstorage/MapDestination.h"
#include "McpSession.h"

#include <functional>

#include <QString>

class ConnectionListener;
class MapData;
class Mmapper2PathMachine;
class McpEventLog;
class McpToolRegistry;
class McpWaiters;

// The parts of MMapper the tools look at. References only; the owner keeps them alive.
struct NODISCARD McpHost final
{
    MapData &mapData;
    ConnectionListener &listener;
    Mmapper2PathMachine &pathMachine;
    QString settingsPath; // looked up once at start-up; see McpService
    // Starts loading a map file in the background. Returns why it could not start, or an empty
    // string; the McpService hears how it went through slot_onMapLoadFinished.
    std::function<QString(const QString &path)> loadMap;
    // Starts saving an independent copy of the map in the background (the map's own file name
    // and saved baseline are left alone). Returns why it could not start, or an empty string;
    // the McpService hears how it went through slot_onMapSaveFinished.
    std::function<QString(const QString &path, SaveModeEnum mode, SaveFormatEnum format)> saveMap;
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
