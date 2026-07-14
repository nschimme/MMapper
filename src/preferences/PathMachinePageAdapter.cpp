// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "PathMachinePageAdapter.h"

#include "../configuration/configuration.h"
#include "../global/utils.h"

PathMachinePageAdapter::PathMachinePageAdapter(QObject *const parent)
    : QObject(parent)
{}

double PathMachinePageAdapter::getAcceptBestRelative() const
{
    return getConfig().pathMachine.acceptBestRelative;
}

void PathMachinePageAdapter::setAcceptBestRelative(const double value)
{
    setConfig().pathMachine.acceptBestRelative = value;
    emit sig_changed();
}

double PathMachinePageAdapter::getAcceptBestAbsolute() const
{
    return getConfig().pathMachine.acceptBestAbsolute;
}

void PathMachinePageAdapter::setAcceptBestAbsolute(const double value)
{
    setConfig().pathMachine.acceptBestAbsolute = value;
    emit sig_changed();
}

double PathMachinePageAdapter::getNewRoomPenalty() const
{
    return getConfig().pathMachine.newRoomPenalty;
}

void PathMachinePageAdapter::setNewRoomPenalty(const double value)
{
    setConfig().pathMachine.newRoomPenalty = value;
    emit sig_changed();
}

double PathMachinePageAdapter::getCorrectPositionBonus() const
{
    return getConfig().pathMachine.correctPositionBonus;
}

void PathMachinePageAdapter::setCorrectPositionBonus(const double value)
{
    setConfig().pathMachine.correctPositionBonus = value;
    emit sig_changed();
}

double PathMachinePageAdapter::getMultipleConnectionsPenalty() const
{
    return getConfig().pathMachine.multipleConnectionsPenalty;
}

void PathMachinePageAdapter::setMultipleConnectionsPenalty(const double value)
{
    setConfig().pathMachine.multipleConnectionsPenalty = value;
    emit sig_changed();
}

int PathMachinePageAdapter::getMaxPaths() const
{
    return getConfig().pathMachine.maxPaths;
}

void PathMachinePageAdapter::setMaxPaths(const int value)
{
    setConfig().pathMachine.maxPaths = utils::clampNonNegative(value);
    emit sig_changed();
}

int PathMachinePageAdapter::getMatchingTolerance() const
{
    return getConfig().pathMachine.matchingTolerance;
}

void PathMachinePageAdapter::setMatchingTolerance(const int value)
{
    setConfig().pathMachine.matchingTolerance = utils::clampNonNegative(value);
    emit sig_changed();
}

void PathMachinePageAdapter::reload()
{
    emit sig_changed();
}
