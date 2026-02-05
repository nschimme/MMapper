#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "../global/macros.h"
#include <QObject>
#include <QColor>

class NODISCARD_QOBJECT GroupPageViewModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY settingsChanged)
    Q_PROPERTY(QColor npcColor READ npcColor WRITE setNpcColor NOTIFY settingsChanged)
    Q_PROPERTY(bool npcColorOverride READ npcColorOverride WRITE setNpcColorOverride NOTIFY settingsChanged)
    Q_PROPERTY(bool npcSortBottom READ npcSortBottom WRITE setNpcSortBottom NOTIFY settingsChanged)
    Q_PROPERTY(bool npcHide READ npcHide WRITE setNpcHide NOTIFY settingsChanged)

public:
    explicit GroupPageViewModel(QObject *parent = nullptr);

    NODISCARD QColor color() const; void setColor(const QColor &v);
    NODISCARD QColor npcColor() const; void setNpcColor(const QColor &v);
    NODISCARD bool npcColorOverride() const; void setNpcColorOverride(bool v);
    NODISCARD bool npcSortBottom() const; void setNpcSortBottom(bool v);
    NODISCARD bool npcHide() const; void setNpcHide(bool v);

    void loadConfig();

signals:
    void settingsChanged();
};
