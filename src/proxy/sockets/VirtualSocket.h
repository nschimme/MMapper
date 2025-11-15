#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors
// Author: Your Name <you@example.com>

#include "../AbstractSocket.h"
#include <QPointer>
#include <QByteArray>

class VirtualSocket final : public AbstractSocket {
    Q_OBJECT

public:
    explicit VirtualSocket(QObject *parent = nullptr);
    ~VirtualSocket() final;

    void flush() override;
    void disconnectFromHost() override;

    void connectToPeer(VirtualSocket *peer);

    qint64 bytesAvailable() const override;

protected:
    qint64 readData(char *data, qint64 maxlen) override;
    qint64 writeData(const char *data, qint64 len) override;

private slots:
    void onPeerDestroyed();

private:
    void writeToPeer(const QByteArray &data);
    QPointer<VirtualSocket> m_peer;
    QByteArray m_buffer;
};
