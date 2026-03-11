#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "../global/ConfigEnums.h"
#include "../global/Signal2.h"
#include "../global/macros.h"

#include <QAbstractNativeEventFilter>
#include <QObject>

class QWindow;

class NODISCARD_QOBJECT ThemeManager final : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT

private:
    Signal2Lifetime m_lifetime;
    ThemeEnum m_appliedThemeSetting = ThemeEnum::Unknown;
    bool m_appliedDarkMode = false;
    bool m_initialized = false;

public:
    explicit ThemeManager(QObject *parent = nullptr);
    ~ThemeManager() override;

public:
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void applyTheme();
    void applyDarkPalette();
    void applyLightPalette();

    void updateAllWindows();
    void applyThemeToWindow(QWindow *window);
    NODISCARD bool isDarkMode() const;
};
