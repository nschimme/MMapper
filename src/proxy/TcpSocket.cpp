// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include "TcpSocket.h"

TcpSocket::TcpSocket(QObject *parent)
    : AbstractSocket(parent)
{
    connect(&m_socket, &QTcpSocket::connected, this, &AbstractSocket::connected);
    connect(&m_socket, &QTcpSocket::disconnected, this, &AbstractSocket::disconnected);
    connect(&m_socket, &QTcpSocket::readyRead, this, &AbstractSocket::readyRead);
    connect(&m_socket, &QTcpSocket::errorOccurred, this, &AbstractSocket::errorOccurred);
}

TcpSocket::~TcpSocket() = default;

void TcpSocket::connectToHost(const QString &hostName, quint16 port)
{
    m_socket.connectToHost(hostName, port);
}

void TcpSocket::disconnectFromHost()
{
    m_socket.disconnectFromHost();
}

qint64 TcpSocket::write(const QByteArray &data)
{
    return m_socket.write(data);
}

bool TcpSocket::setSocketDescriptor(qintptr socketDescriptor)
{
    return m_socket.setSocketDescriptor(socketDescriptor);
}

void TcpSocket::abort()
{
    m_socket.abort();
}

bool TcpSocket::waitForConnected(int msecs)
{
    return m_socket.waitForConnected(msecs);
}

QAbstractSocket::SocketState TcpSocket::state() const
{
    return m_socket.state();
}

QString TcpSocket::errorString() const
{
    return m_socket.errorString();
}

void TcpSocket::setSocketOption(QAbstractSocket::SocketOption option, const QVariant &value)
{
    m_socket.setSocketOption(option, value);
}

void TcpSocket::flush()
{
    m_socket.flush();
}

qint64 TcpSocket::bytesAvailable() const
{
    return m_socket.bytesAvailable();
}

QByteArray TcpSocket::readAll()
{
    return m_socket.readAll();
}
