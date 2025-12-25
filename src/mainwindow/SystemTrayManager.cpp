#include "SystemTrayManager.h"
#include "mainwindow.h"
#include "../configuration/configuration.h"
#include "../group/CGroupChar.h"
#include <QMenu>
#include <QApplication>

SystemTrayManager::SystemTrayManager(MainWindow *parent)
    : QObject{parent}, m_mainWindow(parent)
{
    createActions();
    createTrayIcon();
}

void SystemTrayManager::setVisible(bool visible)
{
    m_trayIcon->setVisible(visible);
}

void SystemTrayManager::onCharacterUpdated(SharedGroupChar character)
{
    if (character && character->isYou()) {
        bool inCombat = (character->getPosition() == CharacterPositionEnum::FIGHTING);
        if (inCombat && !m_inCombat) {
            showCombatNotification();
        }
        m_inCombat = inCombat;
    }
}

void SystemTrayManager::onIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason) {
    case QSystemTrayIcon::Trigger:
    case QSystemTrayIcon::DoubleClick:
        m_mainWindow->show();
        break;
    default:
        ;
    }
}

void SystemTrayManager::onMessageClicked()
{
    m_mainWindow->show();
}

void SystemTrayManager::createActions()
{
    m_openAction = new QAction(tr("&Open"), this);
    connect(m_openAction, &QAction::triggered, m_mainWindow, &MainWindow::show);

    m_preferencesAction = new QAction(tr("&Preferences"), this);
    connect(m_preferencesAction, &QAction::triggered, m_mainWindow, &MainWindow::slot_onPreferences);

    m_quitAction = new QAction(tr("&Quit"), this);
    connect(m_quitAction, &QAction::triggered, qApp, &QApplication::quit);
}

void SystemTrayManager::createTrayIcon()
{
    m_trayMenu = new QMenu(m_mainWindow);
    m_trayMenu->addAction(m_openAction);
    m_trayMenu->addAction(m_preferencesAction);
    m_trayMenu->addSeparator();
    m_trayMenu->addAction(m_quitAction);

    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setContextMenu(m_trayMenu);
    m_trayIcon->setIcon(m_mainWindow->windowIcon());
    m_trayIcon->setToolTip(QApplication::applicationName());

    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &SystemTrayManager::onIconActivated);
    connect(m_trayIcon, &QSystemTrayIcon::messageClicked, this, &SystemTrayManager::onMessageClicked);
}

void SystemTrayManager::showCombatNotification()
{
    const auto& cfg = getConfig();
    bool shouldNotify = false;

    if (cfg.integratedClient.columns > 0) { // Integrated client
        if (!m_mainWindow->isActiveWindow()) {
            shouldNotify = true;
        }
    } else { // Proxy
        if (m_mainWindow->isMinimized()) {
            shouldNotify = true;
        }
    }

    if (shouldNotify) {
        m_trayIcon->showMessage(tr("Combat Started"), tr("You have entered combat."),
                                QSystemTrayIcon::Information, 3000);
    }
}
