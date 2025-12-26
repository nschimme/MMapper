#pragma once

#include "global/Signal2.h"
#include <QObject>
#include <QSystemTrayIcon>
#include "../group/CGroupChar.h"

class QAction;
class QMenu;
class MainWindow;

class SystemTrayManager : public QObject {
    Q_OBJECT

public:
    explicit SystemTrayManager(MainWindow* parent);
    void showCombatNotification();

public slots:
    void onCharacterUpdated(SharedGroupChar character);

private slots:
    void onIconActivated(QSystemTrayIcon::ActivationReason reason);
    void onMessageClicked();

private:
    void updateIconVisibility();

    MainWindow* m_mainWindow;
    QSystemTrayIcon* m_trayIcon;
    QMenu* m_menu;
    QAction* m_openAction;
    QAction* m_preferencesAction;
    QAction* m_quitAction;
    Signal2Lifetime m_lifetime;
    bool m_inCombat = false;
};
