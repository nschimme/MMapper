// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include "VirtualSocket.h"
#include <QTimer>

VirtualSocket::VirtualSocket(QObject *parent)
    : AbstractSocket(parent)
{
}

VirtualSocket::~VirtualSocket() = default;

void VirtualSocket::connectToHost(const QString &hostName, quint16 port)
{
    Q_UNUSED(hostName);
    Q_UNUSED(port);
    if (m_peer) {
        m_state = QAbstractSocket::ConnectedState;
        m_peer->m_state = QAbstractSocket::ConnectedState;
        QTimer::singleShot(0, this, &AbstractSocket::connected);
        QTimer::singleShot(0, m_peer, &AbstractSocket::connected);
    }
}

void VirtualSocket::disconnectFromHost()
{
    if (m_state != QAbstractSocket::UnconnectedState) {
        m_state = QAbstractSocket::UnconnectedState;
        if (m_peer) {
            m_peer->m_state = QAbstractSocket::UnconnectedState;
            QTimer::singleShot(0, m_peer, &AbstractSocket::disconnected);
        }
        QTimer::singleShot(0, this, &AbstractSocket::disconnected);
    }
}

qint64 VirtualSocket::write(const QByteArray &data)
{
    if (m_state == QAbstractSocket::ConnectedState && m_peer) {
        m_peer->receiveData(data);
        return data.size();
    }
    return -1;
}

bool VirtualSocket::setSocketDescriptor(qintptr socketDescriptor)
{
    Q_UNUSED(socketDescriptor);
    return false; // Not applicable for virtual sockets
}

void VirtualSocket::abort()
{
    disconnectFromHost();
}

bool VirtualSocket::waitForConnected(int msecs)
{
    Q_UNUSED(msecs);
    return m_state == QAbstractSocket::ConnectedState;
}

QAbstractSocket::SocketState VirtualSocket::state() const
{
    return m_state;
}

QString VirtualSocket::errorString() const
{
    return "No error";
}

void VirtualSocket::setSocketOption(QAbstractSocket::SocketOption option, const QVariant &value)
{
    Q_UNUSED(option);
    Q_UNUSED(value);
    // Not applicable
}

void VirtualSocket::flush()
{
    // Not applicable
}

qint64 VirtualSocket::bytesAvailable() const
{
    qint64 size = 0;
    for (const auto &data : m_buffer) {
        size += data.size();
    }
    return size;
}

QByteArray VirtualSocket::readAll()
{
    QByteArray result;
    while (!m_buffer.isEmpty()) {
        result.append(m_buffer.dequeue());
    }
    return result;
}

void VirtualSocket::setPeer(VirtualSocket *peer)
{
    m_peer = peer;
}

void VirtualSocket::receiveData(const QByteArray &data)
{
    m_buffer.enqueue(data);
    QTimer::singleShot(0, this, &AbstractSocket::readyRead);
}
