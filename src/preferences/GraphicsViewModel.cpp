// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "GraphicsViewModel.h"
#include "../configuration/configuration.h"

GraphicsViewModel::GraphicsViewModel(QObject *p) : QObject(p) {}

QColor GraphicsViewModel::backgroundColor() const { return getConfig().canvas.backgroundColor.getColor().getQColor(); }
void GraphicsViewModel::setBackgroundColor(const QColor &v) { setConfig().canvas.backgroundColor = Color(v); emit settingsChanged(); }
QColor GraphicsViewModel::roomDarkColor() const { return getConfig().canvas.roomDarkColor.getColor().getQColor(); }
void GraphicsViewModel::setRoomDarkColor(const QColor &v) { setConfig().canvas.roomDarkColor = Color(v); emit settingsChanged(); }
bool GraphicsViewModel::drawNeedsUpdate() const { return getConfig().canvas.showMissingMapId.get(); }
void GraphicsViewModel::setDrawNeedsUpdate(bool v) { setConfig().canvas.showMissingMapId.set(v); emit settingsChanged(); }
bool GraphicsViewModel::drawNotMappedExits() const { return getConfig().canvas.showUnmappedExits.get(); }
void GraphicsViewModel::setDrawNotMappedExits(bool v) { setConfig().canvas.showUnmappedExits.set(v); emit settingsChanged(); }
bool GraphicsViewModel::drawDoorNames() const { return getConfig().canvas.drawDoorNames; }
void GraphicsViewModel::setDrawDoorNames(bool v) { setConfig().canvas.drawDoorNames = v; emit settingsChanged(); }
bool GraphicsViewModel::drawUpperLayersTextured() const { return getConfig().canvas.drawUpperLayersTextured; }
void GraphicsViewModel::setDrawUpperLayersTextured(bool v) { setConfig().canvas.drawUpperLayersTextured = v; emit settingsChanged(); }
QString GraphicsViewModel::resourceDir() const { return getConfig().canvas.resourcesDirectory; }
void GraphicsViewModel::setResourceDir(const QString &v) { setConfig().canvas.resourcesDirectory = v; emit settingsChanged(); }

void GraphicsViewModel::loadConfig() { emit settingsChanged(); }
