// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "QmlBridge.h"

QmlBridge::QmlBridge(MMapperCore &core, QObject *parent)
    : QObject(parent)
    , m_core(core)
{
    connect(&m_core.mapData(), &MapData::sig_onDataChanged, this, &QmlBridge::mapModifiedChanged);
}

void QmlBridge::toggleWidget(const QString &name)
{
    emit requestShowWidget(name);
}

QWidget *QmlBridge::getWidgetByName(const QString &name)
{
    if (name == "room")
        return m_roomWidget;
    if (name == "description")
        return m_descriptionWidget;
    if (name == "group")
        return m_groupWidget;
    if (name == "log")
        return m_logWidget;
    return nullptr;
}
