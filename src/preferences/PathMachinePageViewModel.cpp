// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "PathMachinePageViewModel.h"
#include "../configuration/configuration.h"

PathMachinePageViewModel::PathMachinePageViewModel(QObject *p) : QObject(p) {}

#define CFG_GETSET(Type, Name, Setter, Field) \
Type PathMachinePageViewModel::Name() const { return Field; } \
void PathMachinePageViewModel::Setter(Type v) { if (Field != v) { Field = v; emit settingsChanged(); } }

CFG_GETSET(double, acceptBestRelative, setAcceptBestRelative, setConfig().pathMachine.acceptBestRelative)
CFG_GETSET(double, acceptBestAbsolute, setAcceptBestAbsolute, setConfig().pathMachine.acceptBestAbsolute)
CFG_GETSET(double, newRoomPenalty, setNewRoomPenalty, setConfig().pathMachine.newRoomPenalty)
CFG_GETSET(double, multipleConnectionsPenalty, setMultipleConnectionsPenalty, setConfig().pathMachine.multipleConnectionsPenalty)
CFG_GETSET(double, correctPositionBonus, setCorrectPositionBonus, setConfig().pathMachine.correctPositionBonus)
CFG_GETSET(int, maxPaths, setMaxPaths, setConfig().pathMachine.maxPaths)
CFG_GETSET(int, matchingTolerance, setMatchingTolerance, setConfig().pathMachine.matchingTolerance)

void PathMachinePageViewModel::loadConfig() { emit settingsChanged(); }
