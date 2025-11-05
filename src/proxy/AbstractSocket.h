#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include <QObject>
#include <QByteArray>
#include <QAbstractSocket>
#include <QVariant>

class AbstractSocket : public QObject
{
    Q_OBJECT

public:
    explicit AbstractSocket(QObject *parent = nullptr) : QObject(parent) {}
    ~AbstractSocket() override = default;

    virtual void connectToHost(const QString &hostName, quint16 port) = 0;
    virtual void disconnectFromHost() = 0;
    virtual qint64 write(const QByteArray &data) = 0;
    virtual bool setSocketDescriptor(qintptr socketDescriptor) = 0;
    virtual void abort() = 0;
    virtual bool waitForConnected(int msecs = 30000) = 0;
    virtual QAbstractSocket::SocketState state() const = 0;
    virtual QString errorString() const = 0;
    virtual void setSocketOption(QAbstractSocket::SocketOption option, const QVariant &value) = 0;
    virtual void flush() = 0;
    virtual qint64 bytesAvailable() const = 0;
    virtual QByteArray readAll() = 0;

signals:
    void connected();
    void disconnected();
    void readyRead();
    void errorOccurred(QAbstractSocket::SocketError socketError);
};
