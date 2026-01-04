#pragma once

#include <QObject>
#include <QSettings>
#include <functional>

#include "global/Global.h"

class GroupConfig : public QObject {
    Q_OBJECT

public:
    explicit GroupConfig(const QString &group, QObject *parent = nullptr);

    void set(std::function<void(QSettings &)> callback);
    QSettings *get();

private:
    QString m_group;
    std::function<void(QSettings &)> m_callback;

    void handleChange();
};
