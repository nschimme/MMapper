#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../global/macros.h"
#include "McpAccess.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <set>
#include <vector>

#include <QObject>
#include <QPointer>
#include <QString>
#include <QTcpServer>

class McpConnection;
class McpDispatcher;

// Accepts HTTP connections on the loopback interface and hands each POST /mcp body to a
// McpDispatcher. Everything runs on the thread that owns the server.
//
// Each connection handles one request at a time: a reply may come later (a tool waiting for an
// event), and HTTP/1.1 replies must go out in request order, so pipelined requests wait their
// turn. A tool that never replies is answered with 504 after replyTimeoutMs.
class NODISCARD_QOBJECT McpServer final : public QObject
{
    Q_OBJECT

public:
    struct NODISCARD Options final
    {
        size_t maxConnections = 16;
        int idleTimeoutMs = 60'000;   // no bytes from a client that is not waiting for a reply
        int replyTimeoutMs = 120'000; // upper bound on a deferred reply
    };

private:
    const McpDispatcher &m_dispatcher;
    const Options m_options;
    McpAccessPolicy m_policy;
    std::vector<QPointer<QTcpServer>> m_servers;
    std::set<McpConnection *> m_connections;

public:
    explicit McpServer(const McpDispatcher &dispatcher, QObject *parent, const Options &options);
    explicit McpServer(const McpDispatcher &dispatcher, QObject *parent)
        : McpServer{dispatcher, parent, Options{}}
    {}
    ~McpServer() final;

public:
    // Listens on 127.0.0.1 (and ::1 where available) and requires `token` from clients.
    // Port 0 picks a free port; see port(). Returns an empty string on success, otherwise a
    // description of what went wrong. A failure is not fatal and nothing is left listening.
    NODISCARD QString listen(uint16_t port, const QString &token);

    // Stops listening and drops every connection, including ones waiting for a reply.
    void close();

    NODISCARD bool isListening() const { return !m_servers.empty(); }
    NODISCARD uint16_t port() const { return m_policy.port; }
    NODISCARD size_t connectionCount() const { return m_connections.size(); }

signals:
    void sig_log(const QString &module, const QString &message);

private:
    void slot_onNewConnection(QTcpServer &server);
    void log(const QString &message) { emit sig_log("MCP", message); }
};
