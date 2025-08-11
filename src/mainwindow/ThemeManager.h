#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "../global/macros.h"

#include <QAbstractNativeEventFilter>
#include <QByteArray>
#include <QObject>

class NODISCARD_QOBJECT ThemeManager final : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT

public:
    explicit ThemeManager(QObject *parent = nullptr);
    ~ThemeManager() override;

    void applyTheme();
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    void sig_darkModeChanged(bool dark);

private:
    void applyDarkPalette();
    void applyLightPalette();
    bool isSystemDarkMode();
};
