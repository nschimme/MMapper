#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../global/Signal2.h"
#include "../global/macros.h"
#include "McpBackend.h"
#include "McpDispatcher.h"
#include "McpEventLog.h"
#include "McpServer.h"
#include "McpToolRegistry.h"
#include "McpWaiters.h"

#include <cstdint>
#include <memory>
#include <vector>

#include <QMetaObject>
#include <QObject>
#include <QString>

class GameObserver;

// Owns the MCP server and everything it needs, and keeps it in step with the MCP settings:
// turning it on or off, changing the port or regenerating the token takes effect at once.
// If it cannot listen (say the port is taken) it reports why in the settings and carries on.
class NODISCARD_QOBJECT McpService final : public QObject
{
    Q_OBJECT

private:
    McpHost m_host;
    GameObserver &m_observer;
    McpEventLog m_events;
    McpWaiters m_waiters;
    McpToolRegistry m_registry;
    McpBackend m_backend;
    McpDispatcher m_dispatcher;
    McpServer m_server;
    Signal2Lifetime m_lifetime; // the service's own config subscription

    // Subscriptions that feed the event log. They exist only while the server is listening, so
    // a disabled MCP server costs nothing per event.
    std::unique_ptr<Signal2Lifetime> m_observing;
    std::vector<QMetaObject::Connection> m_qtConnections;

    // What the server was last started with, so unrelated settings changes do nothing.
    bool m_appliedEnabled = false;
    uint16_t m_appliedPort = 0;
    QString m_appliedToken;
    bool m_hasApplied = false;

public:
    // Owned by whoever creates it (not parented), so it can be destroyed at a chosen moment.
    explicit McpService(const McpHost &host, GameObserver &observer);
    ~McpService() final;

public slots:
    // A map load finished; tools that started one are waiting to hear.
    void slot_onMapLoadFinished(const QString &fileName, bool success);

signals:
    void sig_log(const QString &module, const QString &message);

private:
    void applyConfig();
    void setCollecting(bool collect);
};
