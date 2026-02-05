#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "../global/macros.h"
#include <QObject>
#include <QStringList>

class NODISCARD_QOBJECT RoomEditAttrViewModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList roomNames READ roomNames NOTIFY roomNamesChanged)
    Q_PROPERTY(int currentRoomIndex READ currentRoomIndex WRITE setCurrentRoomIndex NOTIFY currentRoomIndexChanged)

public:
    explicit RoomEditAttrViewModel(QObject *parent = nullptr);

    NODISCARD QStringList roomNames() const { return m_roomNames; }
    NODISCARD int currentRoomIndex() const { return m_currentRoomIndex; }
    void setCurrentRoomIndex(int i);

signals:
    void roomNamesChanged();
    void currentRoomIndexChanged();

private:
    QStringList m_roomNames;
    int m_currentRoomIndex = -1;
};
