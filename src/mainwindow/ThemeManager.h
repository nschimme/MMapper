#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "../global/macros.h"

#include <QAbstractNativeEventFilter>
#include <QByteArray>
#include <QObject>

enum class Theme {
    System,
    Light,
    Dark,
};

class NODISCARD_QOBJECT ThemeManager final : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT

private:
    Theme m_theme = Theme::System;

public:
    explicit ThemeManager(QObject *parent = nullptr);
    ~ThemeManager() override;

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

    static bool isSystemInDarkMode();

    bool isDarkMode() const;

    void setTheme(Theme theme);

signals:
    void sig_themeChanged();

private:
    void applyCurrentPalette();
    void applyDarkPalette();
    void applyLightPalette();
};
