// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "ThemeManager.h"

#include "../global/ConfigConsts-Computed.h"

#include <QApplication>
#include <QDebug>
#include <QPalette>
#include <QStyleHints>
#include <QWidget>

#if defined(Q_OS_WIN)
#include <dwmapi.h>
#include <windows.h>
#endif

ThemeManager::ThemeManager(QObject *const parent)
    : QObject(parent)
{
    if constexpr (CURRENT_PLATFORM == PlatformEnum::Windows) {
        qApp->installNativeEventFilter(this);
        qApp->installEventFilter(this);
    }
    applyCurrentPalette();

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(qApp->styleHints(), &QStyleHints::colorSchemeChanged, this, [this]() {
        if (m_theme == Theme::System) {
            applyCurrentPalette();
        }
    });
#endif
}

bool ThemeManager::eventFilter(QObject *watched, QEvent *event)
{
#if defined(Q_OS_WIN)
    if (event->type() == QEvent::Show) {
        QWidget *widget = qobject_cast<QWidget *>(watched);
        if (widget && widget->isWindow() && isDarkMode()) {
            //  Enable dark title bar
            HWND hwnd = reinterpret_cast<HWND>(widget->winId());
            const DWORD DWMWA_USE_IMMERSIVE_DARK_MODE = 20;
            BOOL useDark = TRUE;
            DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDark, sizeof(useDark));
        }
    }
#else
    std::ignore = watched;
    std::ignore = event;
#endif
    return false;
}

ThemeManager::~ThemeManager()
{
    if constexpr (CURRENT_PLATFORM == PlatformEnum::Windows) {
        qApp->removeNativeEventFilter(this);
    }
}

bool ThemeManager::nativeEventFilter(const QByteArray &eventType,
                                     void *message,
                                     qintptr * /*result */)
{
    if (eventType != "windows_generic_MSG")
        return false;

#if defined(Q_OS_WIN)
    MSG *msg = static_cast<MSG *>(message);
    if (msg->message == WM_SETTINGCHANGE && msg->lParam != 0) {
        const wchar_t *param = reinterpret_cast<const wchar_t *>(msg->lParam);
        if (QString::fromWCharArray(param) == "ImmersiveColorSet") {
            applyCurrentPalette();
            emit sig_themeChanged();
        }
    }
#else
    std::ignore = message;
#endif

    return false;
}

void ThemeManager::setTheme(const Theme theme)
{
    m_theme = theme;
    applyCurrentPalette();
    emit sig_themeChanged();
}

bool ThemeManager::isSystemInDarkMode()
{
#if defined(Q_OS_WIN)
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
            return value == 0;
        }
        RegCloseKey(hKey);
    }
    // Return light mode by default
    return false;
#else
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    return qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#else
    return qApp->palette().windowText().color().value() > qApp->palette().window().color().value();
#endif
#endif
}

bool ThemeManager::isDarkMode() const
{
    if (m_theme == Theme::System) {
        return isSystemInDarkMode();
    }
    return m_theme == Theme::Dark;
}


void ThemeManager::applyCurrentPalette()
{
    switch (m_theme) {
    case Theme::System:
        if (isSystemInDarkMode()) {
            applyDarkPalette();
        } else {
            applyLightPalette();
        }
        break;
    case Theme::Light:
        applyLightPalette();
        break;
    case Theme::Dark:
        applyDarkPalette();
        break;
    }
}

void ThemeManager::applyDarkPalette()
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

void ThemeManager::applyLightPalette()
{
    qApp->setPalette(QPalette());
    qApp->setStyle("Fusion");
}
