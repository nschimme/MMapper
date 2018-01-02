#ifndef TILEMANAGER_H
#define TILEMANAGER_H

#include <QObject>

class ThemeReceiver;

class TileManager
{
    Q_OBJECT
public:
    TileManager();

signals:
    void newThemeAvailable();

public slots:
    void addTheme(QString directory);
    void deleteTheme(QString directory);

    void useTheme(QString name);
    void requestTheme(QString name, ThemeReceiver *receiver);

};

#endif // TILEMANAGER_H
