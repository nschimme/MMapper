#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "../global/macros.h"
#include <QObject>
#include <QColor>
#include <QString>

class NODISCARD_QOBJECT GraphicsViewModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor NOTIFY settingsChanged)
    Q_PROPERTY(QColor roomDarkColor READ roomDarkColor WRITE setRoomDarkColor NOTIFY settingsChanged)
    Q_PROPERTY(bool drawNeedsUpdate READ drawNeedsUpdate WRITE setDrawNeedsUpdate NOTIFY settingsChanged)
    Q_PROPERTY(bool drawNotMappedExits READ drawNotMappedExits WRITE setDrawNotMappedExits NOTIFY settingsChanged)
    Q_PROPERTY(bool drawDoorNames READ drawDoorNames WRITE setDrawDoorNames NOTIFY settingsChanged)
    Q_PROPERTY(bool drawUpperLayersTextured READ drawUpperLayersTextured WRITE setDrawUpperLayersTextured NOTIFY settingsChanged)
    Q_PROPERTY(QString resourceDir READ resourceDir WRITE setResourceDir NOTIFY settingsChanged)

public:
    explicit GraphicsViewModel(QObject *parent = nullptr);

    NODISCARD QColor backgroundColor() const; void setBackgroundColor(const QColor &v);
    NODISCARD QColor roomDarkColor() const; void setRoomDarkColor(const QColor &v);
    NODISCARD bool drawNeedsUpdate() const; void setDrawNeedsUpdate(bool v);
    NODISCARD bool drawNotMappedExits() const; void setDrawNotMappedExits(bool v);
    NODISCARD bool drawDoorNames() const; void setDrawDoorNames(bool v);
    NODISCARD bool drawUpperLayersTextured() const; void setDrawUpperLayersTextured(bool v);
    NODISCARD QString resourceDir() const; void setResourceDir(const QString &v);

    void loadConfig();

signals:
    void settingsChanged();
};
