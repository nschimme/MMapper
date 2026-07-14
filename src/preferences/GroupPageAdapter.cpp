// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "GroupPageAdapter.h"

#include "../configuration/configuration.h"

#include <QColorDialog>

GroupPageAdapter::GroupPageAdapter(QWidget *const dialogParent, QObject *const parent)
    : QObject(parent)
    , m_dialogParent(dialogParent)
{}

QColor GroupPageAdapter::getColor() const
{
    return getConfig().groupManager.color;
}

void GroupPageAdapter::setColor(const QColor &value)
{
    setConfig().groupManager.color = value;
    emit sig_changed();
    emit sig_groupSettingsChanged();
}

QColor GroupPageAdapter::getNpcColor() const
{
    return getConfig().groupManager.npcColor;
}

void GroupPageAdapter::setNpcColor(const QColor &value)
{
    setConfig().groupManager.npcColor = value;
    emit sig_changed();
    emit sig_groupSettingsChanged();
}

bool GroupPageAdapter::getNpcColorOverride() const
{
    return getConfig().groupManager.npcColorOverride;
}

void GroupPageAdapter::setNpcColorOverride(const bool value)
{
    setConfig().groupManager.npcColorOverride = value;
    emit sig_changed();
    emit sig_groupSettingsChanged();
}

bool GroupPageAdapter::getNpcSortBottom() const
{
    return getConfig().groupManager.npcSortBottom;
}

void GroupPageAdapter::setNpcSortBottom(const bool value)
{
    setConfig().groupManager.npcSortBottom = value;
    emit sig_changed();
    emit sig_groupSettingsChanged();
}

bool GroupPageAdapter::getNpcHide() const
{
    return getConfig().groupManager.npcHide;
}

void GroupPageAdapter::setNpcHide(const bool value)
{
    setConfig().groupManager.npcHide = value;
    emit sig_changed();
    emit sig_groupSettingsChanged();
}

void GroupPageAdapter::chooseColor()
{
    const QColor color = QColorDialog::getColor(getConfig().groupManager.color,
                                                m_dialogParent,
                                                tr("Select Your Color"));
    if (color.isValid()) {
        setColor(color);
    }
}

void GroupPageAdapter::chooseNpcOverrideColor()
{
    const QColor color = QColorDialog::getColor(getConfig().groupManager.npcColor,
                                                m_dialogParent,
                                                tr("Select NPC Override Color"));
    if (color.isValid()) {
        setNpcColor(color);
    }
}

void GroupPageAdapter::reload()
{
    emit sig_changed();
}
