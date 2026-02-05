#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "../global/macros.h"
#include <QColor>
#include <QFont>
#include <QObject>
#include <QString>

class NODISCARD_QOBJECT DisplayViewModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QColor backgroundColor READ backgroundColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor foregroundColor READ foregroundColor NOTIFY appearanceChanged)
    Q_PROPERTY(QFont font READ font NOTIFY appearanceChanged)
    Q_PROPERTY(int lineLimit READ lineLimit NOTIFY appearanceChanged)

public:
    explicit DisplayViewModel(QObject *parent = nullptr);

    NODISCARD QColor backgroundColor() const;
    NODISCARD QColor foregroundColor() const;
    NODISCARD QFont font() const;
    NODISCARD int lineLimit() const;

    void windowSizeChanged(int cols, int rows);
    void handleBell();
    void returnFocusToInput();
    void showPreview(bool visible);

signals:
    void appearanceChanged();
    void sig_displayText(const QString &text);
    void sig_windowSizeChanged(int cols, int rows);
    void sig_returnFocusToInput();
    void sig_showPreview(bool visible);
    void sig_visualBell();

public slots:
    void slot_displayText(const QString &text);

private:
    void updateAppearance();
};
