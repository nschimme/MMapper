// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "AdvancedGraphicsViewModel.h"
#include "../display/MapCanvasConfig.h"

AdvancedGraphicsViewModel::AdvancedGraphicsViewModel(QObject *p) : QObject(p) {}

bool AdvancedGraphicsViewModel::showPerfStats() const { return MapCanvasConfig::getShowPerfStats(); }
void AdvancedGraphicsViewModel::setShowPerfStats(bool v) { MapCanvasConfig::setShowPerfStats(v); emit settingsChanged(); }
bool AdvancedGraphicsViewModel::mode3d() const { return MapCanvasConfig::isIn3dMode(); }
void AdvancedGraphicsViewModel::setMode3d(bool v) { MapCanvasConfig::set3dMode(v); emit settingsChanged(); }
bool AdvancedGraphicsViewModel::autoTilt() const { return MapCanvasConfig::isAutoTilt(); }
void AdvancedGraphicsViewModel::setAutoTilt(bool v) { MapCanvasConfig::setAutoTilt(v); emit settingsChanged(); }

void AdvancedGraphicsViewModel::loadConfig() { emit settingsChanged(); }
