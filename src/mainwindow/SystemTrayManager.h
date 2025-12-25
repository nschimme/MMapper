#pragma once

#include <QObject>
#include <QSystemTrayIcon>

class QAction;
class QMenu;
class MainWindow;
#include "../group/CGroupChar.h"

class SystemTrayManager : public QObject
{
    Q_OBJECT

public:
    explicit SystemTrayManager(MainWindow *parent);
    void setVisible(bool visible);

public slots:
    void onCharacterUpdated(SharedGroupChar character);

private slots:
    void onIconActivated(QSystemTrayIcon::ActivationReason reason);
    void onMessageClicked();

private:
    void createActions();
    void createTrayIcon();
    void showCombatNotification();

    MainWindow *m_mainWindow;
    QSystemTrayIcon *m_trayIcon;
    QMenu *m_trayMenu;
    QAction *m_openAction;
    QAction *m_preferencesAction;
    QAction *m_quitAction;

    bool m_inCombat = false;
};
