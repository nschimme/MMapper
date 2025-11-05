#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include "AbstractSocket.h"
#include <QTcpSocket>

class TcpSocket : public AbstractSocket
{
    Q_OBJECT

public:
    explicit TcpSocket(QObject *parent = nullptr);
    ~TcpSocket() override;

    void connectToHost(const QString &hostName, quint16 port) override;
    void disconnectFromHost() override;
    qint64 write(const QByteArray &data) override;
    bool setSocketDescriptor(qintptr socketDescriptor) override;
    void abort() override;
    bool waitForConnected(int msecs = 30000) override;
    QAbstractSocket::SocketState state() const override;
    QString errorString() const override;
    void setSocketOption(QAbstractSocket::SocketOption option, const QVariant &value) override;
    void flush() override;
    qint64 bytesAvailable() const override;
    QByteArray readAll() override;

private:
    QTcpSocket m_socket;
};
