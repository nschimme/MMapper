#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "../global/macros.h"
#include <QObject>

class RoomManager;

class NODISCARD_QOBJECT RoomViewModel final : public QObject
{
    Q_OBJECT

public:
    explicit RoomViewModel(RoomManager &rm, QObject *parent = nullptr);

signals:
    void sig_update();

private:
    RoomManager &m_roomManager;
};
