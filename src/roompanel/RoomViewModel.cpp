// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "RoomViewModel.h"
#include "RoomManager.h"

RoomViewModel::RoomViewModel(RoomManager &rm, QObject *parent)
    : QObject(parent), m_roomManager(rm)
{
    connect(&m_roomManager, &RoomManager::sig_updateWidget, this, &RoomViewModel::sig_update);
}
