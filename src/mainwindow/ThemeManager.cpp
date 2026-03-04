// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "ThemeManager.h"

#include "../configuration/configuration.h"
#include "../global/ConfigEnums.h"

#include <QApplication>
#include <QPalette>
#include <QStyle>
#include <QStyleHints>

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#include <QStyleHints>
#endif

#ifdef Q_OS_WIN
#include <dwmapi.h>
#include <windows.h>
#endif

ThemeManager::ThemeManager(QObject *const parent)
    : QObject(parent)
{
    setConfig().general.registerChangeCallback(m_lifetime, [this]() { applyTheme(); });

    if constexpr (CURRENT_PLATFORM == PlatformEnum::Windows) {
        qApp->installNativeEventFilter(this);
        qApp->installEventFilter(this);
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(qApp->styleHints(), &QStyleHints::colorSchemeChanged, this, [this](Qt::ColorScheme) {
        if (getConfig().general.getTheme() == ThemeEnum::System) {
            applyTheme();
        }
    });
#endif

    applyTheme();
}

ThemeManager::~ThemeManager()
{
    if constexpr (CURRENT_PLATFORM == PlatformEnum::Windows) {
        qApp->removeNativeEventFilter(this);
        qApp->removeEventFilter(this);
    }
}

bool ThemeManager::nativeEventFilter(const QByteArray &eventType,
                                     void *message,
                                     qintptr * /*result */)
{
    if (eventType != "windows_generic_MSG")
        return false;

#ifdef Q_OS_WIN
    // REVISIT: Delete this after minimum Qt is 6.5
    MSG *msg = static_cast<MSG *>(message);
    if (msg->message == WM_SETTINGCHANGE && msg->lParam != 0) {
        const wchar_t *param = reinterpret_cast<const wchar_t *>(msg->lParam);
        if (QString::fromWCharArray(param) == "ImmersiveColorSet") {
            applyTheme();
        }
    }
#else
    std::ignore = message;
#endif

    return false;
}

bool ThemeManager::eventFilter(QObject *watched, QEvent *event)
{
#ifdef Q_OS_WIN
    if (event->type() == QEvent::Show || event->type() == QEvent::WinIdChange) {
        QWidget *widget = qobject_cast<QWidget *>(watched);
        if (widget && widget->isWindow()) {
            applyThemeToWindow(widget);
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
    const auto theme = getConfig().general.getTheme();
    if (theme == ThemeEnum::System) {
#if QT_VERSION < QT_VERSION_CHECK(6, 5, 0) && defined(Q_OS_WIN)
        if (isDarkMode()) {
            applyDarkPalette();
        } else {
            qApp->setPalette(QPalette());
            if (qApp->style()->objectName().toLower() != "fusion") {
                qApp->setStyle("Fusion");
            }
        }
#else
        qApp->setPalette(QPalette());
        if (qApp->style()->objectName().toLower() != "fusion") {
            qApp->setStyle("Fusion");
        }
#endif
    } else if (theme == ThemeEnum::Dark) {
        applyDarkPalette();
    } else {
        applyLightPalette();
    }

    if constexpr (CURRENT_PLATFORM == PlatformEnum::Windows) {
        updateAllWindows();
    }
}

void ThemeManager::updateAllWindows()
{
    for (QWidget *widget : QApplication::topLevelWidgets()) {
        applyThemeToWindow(widget);
    }
}

void ThemeManager::applyThemeToWindow(QWidget *widget)
{
#ifdef Q_OS_WIN
    if (widget && widget->isWindow()) {
        HWND hwnd = reinterpret_cast<HWND>(widget->winId());
        const DWORD DWMWA_USE_IMMERSIVE_DARK_MODE = 20;
        BOOL useDark = isDarkMode() ? TRUE : FALSE;
        DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDark, sizeof(useDark));

        // Trigger a frame change to refresh the title bar
        SetWindowPos(hwnd,
                     nullptr,
                     0,
                     0,
                     0,
                     0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    }
#else
    std::ignore = widget;
#endif
}

bool ThemeManager::isDarkMode() const
{
    const auto theme = getConfig().general.getTheme();
    if (theme == ThemeEnum::Dark) {
        return true;
    }
    if (theme == ThemeEnum::Light) {
        return false;
    }

#ifdef Q_OS_WIN
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
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    return qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#else
    return qApp->palette().windowText().color().value() > qApp->palette().window().color().value();
#endif
#endif
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
    dark.setColor(QPalette::Disabled, QPalette::Text, Qt::darkGray);
    dark.setColor(QPalette::Disabled, QPalette::ButtonText, Qt::darkGray);

    qApp->setPalette(dark);
    if (qApp->style()->objectName().toLower() != "fusion") {
        qApp->setStyle("Fusion");
    }
}

void ThemeManager::applyLightPalette()
{
    QPalette light;
    light.setColor(QPalette::Window, QColor(240, 240, 240));
    light.setColor(QPalette::WindowText, Qt::black);
    light.setColor(QPalette::Base, QColor(240, 240, 240));
    light.setColor(QPalette::AlternateBase, QColor(220, 220, 220));
    light.setColor(QPalette::ToolTipBase, QColor(240, 240, 240));
    light.setColor(QPalette::ToolTipText, Qt::black);
    light.setColor(QPalette::Text, Qt::black);
    light.setColor(QPalette::Button, QColor(240, 240, 240));
    light.setColor(QPalette::ButtonText, Qt::black);
    light.setColor(QPalette::BrightText, Qt::red);
    light.setColor(QPalette::Highlight, QColor(0, 120, 215));
    light.setColor(QPalette::HighlightedText, Qt::white);
    light.setColor(QPalette::Disabled, QPalette::Text, Qt::gray);
    light.setColor(QPalette::Disabled, QPalette::ButtonText, Qt::gray);

    qApp->setPalette(light);
    if (qApp->style()->objectName().toLower() != "fusion") {
        qApp->setStyle("Fusion");
    }
}
