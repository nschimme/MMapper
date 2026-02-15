#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "../global/MMapperCore.h"
#include "IMapLoader.h"
#include "QmlBridge.h"

#include <memory>

#include <QQuickWidget>
#include <QWidget>

class MapWindow;
class ClientWidget;
class QDockWidget;

class QmlMainWindow : public QWidget, public IMapLoader
{
    Q_OBJECT
public:
    explicit QmlMainWindow(MMapperCore &core);
    ~QmlMainWindow() override;

    void loadFile(std::shared_ptr<MapSource> source) override;

private:
    void initWidgets();
    void setupQml();

    MMapperCore &m_core;
    QQuickWidget *m_quickWidget = nullptr;
    QmlBridge *m_bridge = nullptr;

    MapWindow *m_mapWindow = nullptr;
    ClientWidget *m_clientWidget = nullptr;

    // Hidden widgets that can be shown via proxy
    QWidget *m_roomWidget = nullptr;
    QWidget *m_descriptionWidget = nullptr;
    QWidget *m_groupWidget = nullptr;
    QWidget *m_logWidget = nullptr;

public slots:
    void slot_open();
    void slot_log(const QString &mod, const QString &message);
};
