#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "../global/MMapperCore.h"

#include <QObject>
#include <QString>

class QmlBridge : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool mapModified READ mapModified NOTIFY mapModifiedChanged)
    Q_PROPERTY(QWidget *mapWidget READ mapWidget WRITE setMapWidget NOTIFY mapWidgetChanged)
    Q_PROPERTY(
        QWidget *clientWidget READ clientWidget WRITE setClientWidget NOTIFY clientWidgetChanged)

public:
    explicit QmlBridge(MMapperCore &core, QObject *parent = nullptr);

    bool mapModified() const { return m_core.mapData().isModified(); }

    QWidget *mapWidget() const { return m_mapWidget; }
    void setMapWidget(QWidget *w)
    {
        if (m_mapWidget != w) {
            m_mapWidget = w;
            emit mapWidgetChanged();
        }
    }

    QWidget *clientWidget() const { return m_clientWidget; }
    void setClientWidget(QWidget *w)
    {
        if (m_clientWidget != w) {
            m_clientWidget = w;
            emit clientWidgetChanged();
        }
    }

    void setSecondaryWidgets(QWidget *room, QWidget *desc, QWidget *group)
    {
        m_roomWidget = room;
        m_descriptionWidget = desc;
        m_groupWidget = group;
    }

    void setLogWidget(QWidget *log) { m_logWidget = log; }

signals:
    void mapWidgetChanged();
    void clientWidgetChanged();

public slots:
    void newFile() { /* implement or call core */ }
    void openFile() { emit requestOpenFile(); }
    void saveFile() { /* ... */ }

    // Bridge to show other widgets
    void toggleWidget(const QString &name);
    QWidget *getWidgetByName(const QString &name);

signals:
    void mapModifiedChanged();
    void requestShowWidget(const QString &name);
    void requestOpenFile();

private:
    MMapperCore &m_core;
    QWidget *m_mapWidget = nullptr;
    QWidget *m_clientWidget = nullptr;
    QWidget *m_roomWidget = nullptr;
    QWidget *m_descriptionWidget = nullptr;
    QWidget *m_groupWidget = nullptr;
    QWidget *m_logWidget = nullptr;
};
