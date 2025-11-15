#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors
// Author: Your Name <you@example.com>

#include "../AbstractSocket.h"
#include <QTcpSocket>

class TcpSocket final : public AbstractSocket
{
    Q_OBJECT

public:
    explicit TcpSocket(qintptr socketDescriptor, QObject *parent = nullptr);
    ~TcpSocket() final;

    void flush() override;
    void disconnectFromHost() override;

    qint64 bytesAvailable() const override;

protected:
    qint64 readData(char *data, qint64 maxlen) override;
    qint64 writeData(const char *data, qint64 len) override;

private:
    QTcpSocket m_socket;
};
