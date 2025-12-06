// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "Theme.h"

#include "../global/ConfigConsts-Computed.h"

#include <QApplication>
#include <QPalette>
#include <QStyleFactory>
#include <QWidget>

#ifdef WIN32
#include <windows.h>
#endif

Theme::Theme(QObject *const parent)
    : QObject(parent)
{
}

Theme::~Theme()
{
}

void Theme::applyTheme(ThemeEnum theme)
{
    if (theme == ThemeEnum::Dark) {
        applyDarkPalette();
    } else if (theme == ThemeEnum::Light) {
        applyLightPalette();
    } else {
        applySystemPalette();
    }
}

bool Theme::isSystemDarkMode()
{
#ifdef WIN32
    DWORD value = 1; // Default to light mode
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                      0,
                      KEY_READ,
                      &hKey)
        == ERROR_SUCCESS) {
        DWORD dataSize = sizeof(value);
        if (RegQueryValueExW(hKey,
                             L"AppsUseLightTheme",
                             nullptr,
                             nullptr,
                             reinterpret_cast<LPBYTE>(&value),
                             &dataSize)
            == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return value == 0; // 0 means dark mode
        }
        RegCloseKey(hKey);
    }
    return false;
#else
    // For other platforms, we can check the Qt palette.
    // This is a simplification and might not cover all desktop environments perfectly.
    return qApp->palette().color(QPalette::Window).lightness() < 128;
#endif
}

void Theme::applySystemPalette()
{
    if (isSystemDarkMode()) {
        applyDarkPalette();
    } else {
        applyLightPalette();
    }
}

void Theme::applyDarkPalette()
{
    QPalette dark;
    dark.setColor(QPalette::Window, QColor(53, 53, 53));
    dark.setColor(QPalette::WindowText, Qt::white);
    dark.setColor(QPalette::Base, QColor(25, 25, 25));
    dark.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
    dark.setColor(QPalette::ToolTipBase, QColor(53, 53, 53));
    dark.setColor(QPalette::ToolTipText, Qt::white);
    dark.setColor(QPalette::Text, Qt::white);
    dark.setColor(QPalette::Button, QColor(53, 53, 53));
    dark.setColor(QPalette::ButtonText, Qt::white);
    dark.setColor(QPalette::BrightText, Qt::red);
    dark.setColor(QPalette::Highlight, QColor(142, 45, 197).lighter());
    dark.setColor(QPalette::HighlightedText, Qt::black);

    qApp->setPalette(dark);
    qApp->setStyle("Fusion");
}

void Theme::applyLightPalette()
{
    qApp->setPalette(QApplication::style()->standardPalette());
    qApp->setStyle(QStyleFactory::keys().contains("Windows") ? "Windows" : "Fusion");
}
