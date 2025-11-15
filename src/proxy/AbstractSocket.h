#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors
// Author: Your Name <you@example.com>

#include <QIODevice>

class AbstractSocket : public QIODevice
{
    Q_OBJECT

public:
    explicit AbstractSocket(QObject *parent = nullptr) : QIODevice(parent) {}
    ~AbstractSocket() override = default;

    virtual void flush() = 0;
    virtual void disconnectFromHost() = 0;

signals:
    void connected();
    void disconnected();
};
