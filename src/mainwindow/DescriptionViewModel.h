#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "../map/RoomHandle.h"
#include "../global/macros.h"
#include <QColor>
#include <QObject>
#include <QString>
#include <map>
#include <set>

class NODISCARD_QOBJECT DescriptionViewModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString backgroundImagePath READ backgroundImagePath NOTIFY backgroundImagePathChanged)
    Q_PROPERTY(QString roomName READ roomName NOTIFY roomNameChanged)
    Q_PROPERTY(QString roomDescription READ roomDescription NOTIFY roomDescriptionChanged)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor roomNameColor READ roomNameColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor roomDescColor READ roomDescColor NOTIFY colorsChanged)

public:
    explicit DescriptionViewModel(QObject *parent = nullptr);

    NODISCARD QString backgroundImagePath() const { return m_backgroundImagePath; }
    NODISCARD QString roomName() const { return m_roomName; }
    NODISCARD QString roomDescription() const { return m_roomDescription; }
    NODISCARD QColor backgroundColor() const;
    NODISCARD QColor roomNameColor() const;
    NODISCARD QColor roomDescColor() const;

    void updateRoom(const RoomHandle &r);
    void scanDirectories();

signals:
    void backgroundImagePathChanged();
    void roomNameChanged();
    void roomDescriptionChanged();
    void colorsChanged();

private:
    void updateData();
    NODISCARD QColor toColor(const QString &str) const;

    RoomHandle m_room;
    QString m_backgroundImagePath;
    QString m_roomName;
    QString m_roomDescription;
    std::map<QString, QString> m_availableFiles;
};
