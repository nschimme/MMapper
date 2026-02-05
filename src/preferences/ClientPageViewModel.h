#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "../global/macros.h"
#include <QObject>
#include <QColor>
#include <QFont>

class NODISCARD_QOBJECT ClientPageViewModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QFont font READ font WRITE setFont NOTIFY settingsChanged)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor NOTIFY settingsChanged)
    Q_PROPERTY(QColor foregroundColor READ foregroundColor WRITE setForegroundColor NOTIFY settingsChanged)
    Q_PROPERTY(int columns READ columns WRITE setColumns NOTIFY settingsChanged)
    Q_PROPERTY(int rows READ rows WRITE setRows NOTIFY settingsChanged)

public:
    explicit ClientPageViewModel(QObject *parent = nullptr);

    NODISCARD QFont font() const;
    void setFont(const QFont &v);
    NODISCARD QColor backgroundColor() const;
    void setBackgroundColor(const QColor &v);
    NODISCARD QColor foregroundColor() const;
    void setForegroundColor(const QColor &v);
    NODISCARD int columns() const;
    void setColumns(int v);
    NODISCARD int rows() const;
    void setRows(int v);

    void loadConfig();

signals:
    void settingsChanged();
};
