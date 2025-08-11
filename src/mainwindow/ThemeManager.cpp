// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "ThemeManager.h"

#include "../configuration/configuration.h"
#include "../global/ConfigConsts-Computed.h"

#include <QApplication>
#include <QDebug>
#include <QPalette>
#include <QWidget>

#if defined(Q_OS_WIN)
#include <dwmapi.h>
#include <windows.h>
#elif defined(Q_OS_MACOS)
#include <QProcess>
#endif

ThemeManager::ThemeManager(QObject *const parent)
    : QObject(parent)
{
#if defined(Q_OS_WIN)
    qApp->installNativeEventFilter(this);
    qApp->installEventFilter(this);
#endif
}

ThemeManager::~ThemeManager()
{
#if defined(Q_OS_WIN)
    qApp->removeNativeEventFilter(this);
#endif
}

bool ThemeManager::nativeEventFilter(const QByteArray &eventType,
                                     void *message,
                                     qintptr * /*result */)
{
#if defined(Q_OS_WIN)
    if (eventType != "windows_generic_MSG")
        return false;

    MSG *msg = static_cast<MSG *>(message);
    if (msg->message == WM_SETTINGCHANGE && msg->lParam != 0) {
        const wchar_t *param = reinterpret_cast<const wchar_t *>(msg->lParam);
        if (QString::fromWCharArray(param) == "ImmersiveColorSet") {
            applyTheme();
        }
    }
#else
    std::ignore = eventType;
    std::ignore = message;
#endif

    return false;
}

bool ThemeManager::eventFilter(QObject *watched, QEvent *event)
{
#if defined(Q_OS_WIN)
    if (event->type() == QEvent::Show) {
        QWidget *widget = qobject_cast<QWidget *>(watched);
        if (widget && widget->isWindow() && getConfig().general.darkMode != DarkMode::Light) {
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

void ThemeManager::applyTheme()
{
    const auto darkMode = getConfig().general.darkMode;

    bool useDarkMode = false;
    if (darkMode == DarkMode::Auto) {
        useDarkMode = isSystemDarkMode();
    } else if (darkMode == DarkMode::Dark) {
        useDarkMode = true;
    }

    if (useDarkMode) {
        applyDarkPalette();
    } else {
        applyLightPalette();
    }

    emit sig_darkModeChanged(useDarkMode);
}

bool ThemeManager::isSystemDarkMode()
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
            return value == 0; // 0 means dark mode
        }
        RegCloseKey(hKey);
    }
#elif defined(Q_OS_MACOS)
    QProcess process;
    process.start("defaults", {"read", "-g", "AppleInterfaceStyle"});
    process.waitForFinished(1000);
    return process.readAllStandardOutput().trimmed() == "Dark";
#elif defined(Q_OS_LINUX)
    return qApp->palette().color(QPalette::WindowText).lightness()
           > qApp->palette().color(QPalette::Window).lightness();
#endif
    return false;
}

void ThemeManager::applyDarkPalette()
{
    QPalette dark;
    dark.setColor(QPalette::Window, QColor(53, 53, 53));
    dark.setColor(QPalette::WindowText, Qt::white);
    dark.setColor(QPalette::Base, QColor(25, 25, 25));
    dark.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
    dark.setColor(QPalette::ToolTipBase, Qt::white);
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
