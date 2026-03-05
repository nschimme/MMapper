// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "ThemeManager.h"

#include "../configuration/configuration.h"
#include "../global/ConfigEnums.h"

#include <QApplication>
#include <QMenuBar>
#include <QMessageBox>
#include <QPalette>
#include <QStyle>
#include <QStyleHints>
#include <QWindow>

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
    const auto themeSetting = getConfig().general.getTheme();
    const bool darkMode = isDarkMode();

    if (m_initialized && CURRENT_PLATFORM == PlatformEnum::Windows) {
        if (themeSetting != m_appliedThemeSetting || darkMode != m_appliedDarkMode) {
            QMessageBox::information(QApplication::activeWindow(),
                                     tr("Theme Change"),
                                     tr("Please restart MMapper for the theme changes to take "
                                        "effect completely."));
        }
        return;
    }

    m_appliedThemeSetting = themeSetting;
    m_appliedDarkMode = darkMode;

    if (themeSetting == ThemeEnum::System) {
#if QT_VERSION < QT_VERSION_CHECK(6, 5, 0) && defined(Q_OS_WIN)
        if (darkMode) {
            applyDarkPalette();
        } else {
            qApp->setPalette(QPalette());
            if (qApp->style()->objectName().compare("fusion", Qt::CaseInsensitive) != 0) {
                qApp->setStyle("Fusion");
            }
        }
#else
        qApp->setPalette(QPalette());
        if (qApp->style()->objectName().compare("fusion", Qt::CaseInsensitive) != 0) {
            qApp->setStyle("Fusion");
        }
#endif
    } else if (themeSetting == ThemeEnum::Dark) {
        applyDarkPalette();
    } else {
        applyLightPalette();
    }

    // Explicitly update ALL widgets because Fusion style/Windows can be stubborn
    // especially with the MenuBar and ToolBars.
    for (auto *widget : QApplication::allWidgets()) {
        widget->setPalette(qApp->palette());
        widget->update();
    }

    if constexpr (CURRENT_PLATFORM == PlatformEnum::Windows) {
        updateAllWindows();
    }

    // Force redraw of all canvases and their containers to avoid frozen states
    for (QWindow *window : QGuiApplication::allWindows()) {
        window->requestUpdate();
    }

    m_initialized = true;
}

void ThemeManager::updateAllWindows()
{
    for (QWindow *window : QGuiApplication::topLevelWindows()) {
        applyThemeToWindow(window);
    }
}

void ThemeManager::applyThemeToWindow(QWindow *window)
{
#ifdef Q_OS_WIN
    if (window && window->isTopLevel()) {
        HWND hwnd = reinterpret_cast<HWND>(window->winId());
        BOOL useDark = isDarkMode() ? TRUE : FALSE;

        // Windows 11 and Windows 10 20H1+
        const DWORD DWMWA_USE_IMMERSIVE_DARK_MODE = 20;
        DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDark, sizeof(useDark));

        // Windows 10 older versions
        const DWORD DWMWA_USE_IMMERSIVE_DARK_MODE_OLD = 19;
        DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE_OLD, &useDark, sizeof(useDark));

        // Dark mode corner preference (Windows 11)
        const DWORD DWMWA_WINDOW_CORNER_PREFERENCE = 33;
        const DWORD DWMWCP_ROUND = 2;
        DwmSetWindowAttribute(hwnd,
                              DWMWA_WINDOW_CORNER_PREFERENCE,
                              &DWMWCP_ROUND,
                              sizeof(DWMWCP_ROUND));

        // Trigger a frame change to refresh the title bar
        SetWindowPos(hwnd,
                     nullptr,
                     0,
                     0,
                     0,
                     0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_DRAWFRAME);
    }
#else
    std::ignore = window;
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
    if (qApp->style()->objectName().compare("fusion", Qt::CaseInsensitive) != 0) {
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
    if (qApp->style()->objectName().compare("fusion", Qt::CaseInsensitive) != 0) {
        qApp->setStyle("Fusion");
    }
}
