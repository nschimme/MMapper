// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "RoomEditAttrViewModel.h"

RoomEditAttrViewModel::RoomEditAttrViewModel(QObject *p) : QObject(p) {}
void RoomEditAttrViewModel::setCurrentRoomIndex(int i) { if (m_currentRoomIndex != i) { m_currentRoomIndex = i; emit currentRoomIndexChanged(); } }
