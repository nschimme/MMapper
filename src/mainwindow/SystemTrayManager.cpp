#include "SystemTrayManager.h"
#include "mainwindow.h"
#include "configuration/configuration.h"
#include "global/ChangeMonitor.h"
#include <QMenu>
#include <QApplication>
#include <QTimer>

SystemTrayManager::SystemTrayManager(MainWindow* parent)
    : QObject(parent), m_mainWindow(parent) {
    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(QIcon(":/images/m-logo.png"));
    m_trayIcon->setToolTip("MMapper");

    m_menu = new QMenu(parent);
    m_openAction = m_menu->addAction("Open");
    m_preferencesAction = m_menu->addAction("Preferences");
    m_quitAction = m_menu->addAction("Quit");
    m_trayIcon->setContextMenu(m_menu);

    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &SystemTrayManager::onIconActivated);
    connect(m_trayIcon, &QSystemTrayIcon::messageClicked, this, &SystemTrayManager::onMessageClicked);
    connect(m_openAction, &QAction::triggered, m_mainWindow, &MainWindow::show);
    connect(m_preferencesAction, &QAction::triggered, m_mainWindow, &MainWindow::slot_onPreferences);
    connect(m_quitAction, &QAction::triggered, qApp, &QApplication::quit);

    setConfig().general.registerChangeCallback(m_lifetime, [this] {
        updateIconVisibility();
    });

    QTimer::singleShot(0, this, &SystemTrayManager::updateIconVisibility);
}

void SystemTrayManager::onIconActivated(QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::Trigger) {
        m_mainWindow->showNormal();
        m_mainWindow->activateWindow();
    }
}

void SystemTrayManager::onMessageClicked() {
    m_mainWindow->showNormal();
    m_mainWindow->activateWindow();
}

void SystemTrayManager::onCharacterUpdated(SharedGroupChar character) {
    if (!character) {
        return;
    }

    bool inCombat = character->getPosition() == CharacterPositionEnum::FIGHTING;
    if (inCombat && !m_inCombat) {
        bool isProxy = !m_mainWindow->isUsingIntegratedClient();
        bool shouldNotify = false;

        if (isProxy) {
            shouldNotify = m_mainWindow->isMinimized();
        } else {
            shouldNotify = !m_mainWindow->isActiveWindow();
        }

        if (shouldNotify) {
            showCombatNotification();
        }
    }
    m_inCombat = inCombat;
}

void SystemTrayManager::showCombatNotification() {
    if (!m_trayIcon->isVisible()) {
        return;
    }
    m_trayIcon->showMessage("Combat Started", "You have entered combat.", QSystemTrayIcon::Information, 3000);
}

void SystemTrayManager::updateIconVisibility() {
    bool hideToTray = getConfig().general.getHideToSystemTray();
    m_trayIcon->setVisible(hideToTray);

    if (!hideToTray && !m_mainWindow->isVisible()) {
        m_mainWindow->show();
    }
}
