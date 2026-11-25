// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "McpServer.h"

#include "McpDispatcher.h"
#include "McpHttp.h"

#include <optional>
#include <utility>

#include <QHostAddress>
#include <QMetaObject>
#include <QTcpSocket>
#include <QTimer>

// One client connection. Owned by the McpServer; deletes itself when the socket goes away.
class McpConnection final : public QObject
{
private:
    const McpDispatcher &m_dispatcher;
    const McpAccessPolicy &m_policy;
    const McpServer::Options m_options;
    QTcpSocket *const m_socket;
    McpHttpParser m_parser;
    QTimer m_idleTimer;
    QTimer m_replyTimer;
    bool m_busy = false; // a request is with the dispatcher and has not been answered yet
    uint64_t m_requestId = 0;

public:
    McpConnection(const McpDispatcher &dispatcher,
                  const McpAccessPolicy &policy,
                  const McpServer::Options &options,
                  QTcpSocket *const socket,
                  QObject *const parent)
        : QObject{parent}
        , m_dispatcher{dispatcher}
        , m_policy{policy}
        , m_options{options}
        , m_socket{socket}
    {
        m_socket->setParent(this);

        m_idleTimer.setSingleShot(true);
        m_replyTimer.setSingleShot(true);
        connect(&m_idleTimer, &QTimer::timeout, this, [this]() { closeNow(); });
        connect(&m_replyTimer, &QTimer::timeout, this, [this]() { onReplyTimeout(); });
        connect(m_socket, &QTcpSocket::readyRead, this, [this]() { onReadyRead(); });
        connect(m_socket, &QTcpSocket::disconnected, this, [this]() { deleteLater(); });

        m_idleTimer.start(m_options.idleTimeoutMs);
    }
    ~McpConnection() final;

private:
    void closeNow()
    {
        m_socket->abort();
        deleteLater();
    }

    void onReadyRead()
    {
        if (!m_busy) {
            m_idleTimer.start(m_options.idleTimeoutMs);
        }
        m_parser.feed(m_socket->readAll());

        // A client that keeps sending while it waits must not make us buffer without limit.
        constexpr size_t slack = 2;
        const auto limits = McpHttpParser::Limits{};
        if (m_parser.bufferedBytes() > slack * (limits.maxBodyBytes + limits.maxHeaderBytes)) {
            closeNow();
            return;
        }
        pump();
    }

    // Handles queued requests until one has to wait for its reply.
    void pump()
    {
        while (!m_busy && m_socket->state() == QAbstractSocket::ConnectedState) {
            const auto next = m_parser.next();
            if (!next.has_value()) {
                return;
            }
            if (const auto *err = std::get_if<McpHttpError>(&*next)) {
                // The stream can't be trusted any more, so reply and hang up.
                send(err->status, "text/plain", err->reason, false, err->headers);
                return;
            }
            handle(std::get<McpHttpRequest>(*next));
        }
    }

    void handle(const McpHttpRequest &request)
    {
        if (const auto err = mcpCheckRequest(request, m_policy)) {
            // The request body was fully read, so the connection can be reused.
            send(err->status, "text/plain", err->reason, request.keepAlive, err->headers);
            return;
        }

        m_busy = true;
        m_idleTimer.stop();
        m_replyTimer.start(m_options.replyTimeoutMs);
        const uint64_t id = ++m_requestId;
        const bool keepAlive = request.keepAlive;

        // The reply may arrive after this connection is gone; QPointer makes that harmless.
        QPointer<McpConnection> self{this};
        m_dispatcher.handle(request.body, [self, id, keepAlive](std::optional<QByteArray> body) {
            if (self) {
                self->onReply(id, keepAlive, std::move(body));
            }
        });
    }

    void onReply(const uint64_t id, const bool keepAlive, std::optional<QByteArray> body)
    {
        if (!m_busy || id != m_requestId) {
            return; // stale: timed out already, or answered twice
        }
        m_busy = false;
        m_replyTimer.stop();

        if (body.has_value()) {
            send(200, "application/json", *body, keepAlive);
        } else {
            send(202, {}, {}, keepAlive); // a notification; nothing to send back
        }

        // Serve whatever queued up behind it, but not from inside the dispatcher's call stack.
        QMetaObject::invokeMethod(this, [this]() { pump(); }, Qt::QueuedConnection);
    }

    void onReplyTimeout()
    {
        if (!m_busy) {
            return;
        }
        m_busy = false;
        ++m_requestId; // a late reply from the tool is now stale
        send(504, "text/plain", "the tool did not reply in time", false);
    }

    void send(const int status,
              const QByteArray &contentType,
              const QByteArray &body,
              const bool keepAlive,
              const std::map<QByteArray, QByteArray> &headers = {})
    {
        m_socket->write(mcpHttpResponse(status, contentType, body, keepAlive, headers));
        if (keepAlive) {
            m_idleTimer.start(m_options.idleTimeoutMs);
        } else {
            m_idleTimer.stop();
            m_socket->disconnectFromHost(); // closes once the reply has been written
        }
    }
};

McpConnection::~McpConnection() = default;

McpServer::McpServer(const McpDispatcher &dispatcher, QObject *const parent, const Options &options)
    : QObject{parent}
    , m_dispatcher{dispatcher}
    , m_options{options}
{}

McpServer::~McpServer()
{
    close();
}

QString McpServer::listen(const uint16_t port, const QString &token)
{
    close();
    if (token.isEmpty()) {
        return QStringLiteral("refusing to listen without a token");
    }

    auto makeServer = [this]() {
        auto *const server = new QTcpServer(this);
        connect(server, &QTcpServer::newConnection, this, [this, server]() {
            slot_onNewConnection(*server);
        });
        return server;
    };

    auto *const v4 = makeServer();
    if (!v4->listen(QHostAddress::LocalHost, port)) {
        const QString why = v4->errorString();
        delete v4;
        return why;
    }
    m_servers.emplace_back(v4);
    m_policy.port = v4->serverPort(); // the real port when 0 was asked for
    m_policy.token = token.toUtf8();

    // "localhost" may resolve to ::1 first. Not every machine has IPv6, so this one may fail.
    auto *const v6 = makeServer();
    if (v6->listen(QHostAddress::LocalHostIPv6, m_policy.port)) {
        m_servers.emplace_back(v6);
    } else {
        log(QStringLiteral("not listening on ::1: ") + v6->errorString());
        delete v6;
    }

    log(QStringLiteral("listening on 127.0.0.1:%1").arg(m_policy.port));
    return {};
}

void McpServer::close()
{
    for (auto &server : m_servers) {
        if (server) {
            server->close();
            delete server;
        }
    }
    m_servers.clear();

    // Deleting a connection removes it from m_connections (see slot_onNewConnection).
    const auto connections = m_connections;
    for (McpConnection *const connection : connections) {
        delete connection;
    }
    m_policy = McpAccessPolicy{};
}

void McpServer::slot_onNewConnection(QTcpServer &server)
{
    while (server.hasPendingConnections()) {
        QTcpSocket *const socket = server.nextPendingConnection();
        if (socket == nullptr) {
            return;
        }
        if (m_connections.size() >= m_options.maxConnections) {
            log(QStringLiteral("too many connections; dropping one"));
            socket->abort();
            socket->deleteLater();
            continue;
        }
        auto *const connection = new McpConnection(m_dispatcher, m_policy, m_options, socket, this);
        m_connections.insert(connection);
        connect(connection, &QObject::destroyed, this, [this, connection]() {
            m_connections.erase(connection);
        });
    }
}
