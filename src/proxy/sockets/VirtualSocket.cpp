// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors
// Author: Your Name <you@example.com>

#include "VirtualSocket.h"
#include <QTimer>
#include <algorithm>

VirtualSocket::VirtualSocket(QObject *parent) : AbstractSocket(parent) {
    open(QIODevice::ReadWrite);
}

VirtualSocket::~VirtualSocket() {
    disconnectFromHost();
}

void VirtualSocket::connectToPeer(VirtualSocket *peer) {
    m_peer = peer;
    peer->m_peer = this;
    QObject::connect(peer, &QObject::destroyed, this, &VirtualSocket::onPeerDestroyed);
    emit connected();
    emit peer->connected();
}

void VirtualSocket::flush() {}

void VirtualSocket::disconnectFromHost() {
    if (m_peer) {
        emit disconnected();
        m_peer->m_peer = nullptr;
        m_peer = nullptr;
    }
}

void VirtualSocket::onPeerDestroyed() {
    if (m_peer) {
        m_peer = nullptr;
        emit disconnected();
    }
}

void VirtualSocket::writeToPeer(const QByteArray &data) {
    m_buffer.append(data);
    QTimer::singleShot(0, this, [this]() {
        emit readyRead();
    });
}

qint64 VirtualSocket::bytesAvailable() const
{
    return m_buffer.size();
}

qint64 VirtualSocket::readData(char *data, qint64 maxlen)
{
    qint64 len = std::min(maxlen, (qint64)m_buffer.size());
    memcpy(data, m_buffer.constData(), len);
    m_buffer.remove(0, len);
    return len;
}

qint64 VirtualSocket::writeData(const char *data, qint64 len)
{
    if (m_peer) {
        m_peer->writeToPeer(QByteArray(data, len));
    }
    return len;
}
