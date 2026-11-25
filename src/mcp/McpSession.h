#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../global/macros.h"

#include <memory>

#include <QPointer>
#include <QString>

class ConnectionListener;
class Proxy;
class VirtualSocket;

// A client session that MCP can start without any telnet client.
//
// MMapper serves one client at a time, and only while a client is connected is there a Proxy
// (the map, parser and MUD connection are all wired up through it). The built-in client gets one
// by connecting a VirtualSocket pair to the listener; this does the same, so a test can start a
// session on its own. What MMapper writes to the "client" is read and thrown away here: the
// events reach MCP through the game observer, and an unread socket would just keep growing.
//
// A session that belongs to someone else (a real client) is never created over, and never closed.
class NODISCARD McpSession final
{
public:
    // Closing: MCP ended its session and MMapper is still tearing the proxy down.
    enum class OwnerEnum { None, Mcp, Client, Closing };

    struct NODISCARD OpenResult final
    {
        QString error;        // empty on success
        bool created = false; // false if there was already a session and we attached to it
    };

private:
    ConnectionListener &m_listener;
    std::unique_ptr<VirtualSocket> m_socket;
    QPointer<Proxy> m_ownedProxy; // the proxy our session created

public:
    explicit McpSession(ConnectionListener &listener);
    ~McpSession();
    McpSession(const McpSession &) = delete;
    McpSession &operator=(const McpSession &) = delete;

    NODISCARD OwnerEnum owner() const;

    // Starts a session, or reports the one that already exists.
    NODISCARD OpenResult open();

    // Ends a session MCP started. Returns an error for one it did not start.
    NODISCARD QString close();
};

NODISCARD const char *mcpOwnerName(McpSession::OwnerEnum owner);
