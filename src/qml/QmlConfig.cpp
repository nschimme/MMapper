// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "QmlConfig.h"

#include "../configuration/configuration.h"

QmlConfig::QmlConfig(QObject *const parent)
    : QObject(parent)
    , m_npcHide(getConfig().groupManager.npcHide)
    , m_npcSortBottom(getConfig().groupManager.npcSortBottom)
    , m_groupColor(getConfig().groupManager.color)
{}

bool QmlConfig::getNpcHide() const
{
    return getConfig().groupManager.npcHide;
}

void QmlConfig::setNpcHide(const bool value)
{
    if (getConfig().groupManager.npcHide == value) {
        return;
    }
    setConfig().groupManager.npcHide = value;
    m_npcHide = value;
    emit npcHideChanged();
}

bool QmlConfig::getNpcSortBottom() const
{
    return getConfig().groupManager.npcSortBottom;
}

void QmlConfig::setNpcSortBottom(const bool value)
{
    if (getConfig().groupManager.npcSortBottom == value) {
        return;
    }
    setConfig().groupManager.npcSortBottom = value;
    m_npcSortBottom = value;
    emit npcSortBottomChanged();
}

QColor QmlConfig::getGroupColor() const
{
    return getConfig().groupManager.color;
}

void QmlConfig::setGroupColor(const QColor &value)
{
    if (getConfig().groupManager.color == value) {
        return;
    }
    setConfig().groupManager.color = value;
    m_groupColor = value;
    emit groupColorChanged();
}

void QmlConfig::reload()
{
    if (const bool value = getConfig().groupManager.npcHide; value != m_npcHide) {
        m_npcHide = value;
        emit npcHideChanged();
    }
    if (const bool value = getConfig().groupManager.npcSortBottom; value != m_npcSortBottom) {
        m_npcSortBottom = value;
        emit npcSortBottomChanged();
    }
    if (const QColor value = getConfig().groupManager.color; value != m_groupColor) {
        m_groupColor = value;
        emit groupColorChanged();
    }
}
