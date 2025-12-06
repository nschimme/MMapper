#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "../global/macros.h"
#include "../global/ConfigEnums.h"

#include <QObject>

class NODISCARD_QOBJECT Theme final : public QObject
{
    Q_OBJECT

public:
    explicit Theme(QObject *parent = nullptr);
    ~Theme() override;

    static void applyTheme(ThemeEnum theme);

private:
    static void applyDarkPalette();
    static void applyLightPalette();
    static void applySystemPalette();
    static bool isSystemDarkMode();
};
