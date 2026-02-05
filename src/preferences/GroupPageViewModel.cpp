// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "GroupPageViewModel.h"
#include "../configuration/configuration.h"

GroupPageViewModel::GroupPageViewModel(QObject *p) : QObject(p) {}

QColor GroupPageViewModel::color() const { return getConfig().groupManager.color; }
void GroupPageViewModel::setColor(const QColor &v) { if (getConfig().groupManager.color != v) { setConfig().groupManager.color = v; emit settingsChanged(); } }
QColor GroupPageViewModel::npcColor() const { return getConfig().groupManager.npcColor; }
void GroupPageViewModel::setNpcColor(const QColor &v) { if (getConfig().groupManager.npcColor != v) { setConfig().groupManager.npcColor = v; emit settingsChanged(); } }
bool GroupPageViewModel::npcColorOverride() const { return getConfig().groupManager.npcColorOverride; }
void GroupPageViewModel::setNpcColorOverride(bool v) { if (getConfig().groupManager.npcColorOverride != v) { setConfig().groupManager.npcColorOverride = v; emit settingsChanged(); } }
bool GroupPageViewModel::npcSortBottom() const { return getConfig().groupManager.npcSortBottom; }
void GroupPageViewModel::setNpcSortBottom(bool v) { if (getConfig().groupManager.npcSortBottom != v) { setConfig().groupManager.npcSortBottom = v; emit settingsChanged(); } }
bool GroupPageViewModel::npcHide() const { return getConfig().groupManager.npcHide; }
void GroupPageViewModel::setNpcHide(bool v) { if (getConfig().groupManager.npcHide != v) { setConfig().groupManager.npcHide = v; emit settingsChanged(); } }

void GroupPageViewModel::loadConfig() { emit settingsChanged(); }
