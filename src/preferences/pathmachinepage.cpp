// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "pathmachinepage.h"

#include "../configuration/configuration.h"
#include "../global/SignalBlocker.h"

#include <QDoubleSpinBox>

PathmachinePage::PathmachinePage(QWidget *parent)
    : QWidget(parent)
{
    setupUi(this);

    connect(acceptBestRelativeDoubleSpinBox,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this,
            &PathmachinePage::slot_acceptBestRelativeDoubleSpinBoxValueChanged);
    connect(acceptBestAbsoluteDoubleSpinBox,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this,
            &PathmachinePage::slot_acceptBestAbsoluteDoubleSpinBoxValueChanged);
    connect(newRoomPenaltyDoubleSpinBox,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this,
            &PathmachinePage::slot_newRoomPenaltyDoubleSpinBoxValueChanged);
    connect(correctPositionBonusDoubleSpinBox,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this,
            &PathmachinePage::slot_correctPositionBonusDoubleSpinBoxValueChanged);
    connect(multipleConnectionsPenaltyDoubleSpinBox,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this,
            &PathmachinePage::slot_multipleConnectionsPenaltyDoubleSpinBoxValueChanged);

    connect(maxPaths,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &PathmachinePage::slot_maxPathsValueChanged);
    connect(matchingToleranceSpinBox,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &PathmachinePage::slot_matchingToleranceSpinBoxValueChanged);

    auto &pm = setConfig().pathMachine;
    pm.acceptBestRelative.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
    pm.acceptBestAbsolute.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
    pm.newRoomPenalty.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
    pm.correctPositionBonus.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
    pm.multipleConnectionsPenalty.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
    pm.maxPaths.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
    pm.matchingTolerance.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
}

void PathmachinePage::slot_loadConfig()
{
    SignalBlocker b1(*acceptBestRelativeDoubleSpinBox);
    SignalBlocker b2(*acceptBestAbsoluteDoubleSpinBox);
    SignalBlocker b3(*newRoomPenaltyDoubleSpinBox);
    SignalBlocker b4(*correctPositionBonusDoubleSpinBox);
    SignalBlocker b5(*multipleConnectionsPenaltyDoubleSpinBox);
    SignalBlocker b6(*maxPaths);
    SignalBlocker b7(*matchingToleranceSpinBox);

    const auto &settings = getConfig().pathMachine;
    acceptBestRelativeDoubleSpinBox->setValue(settings.acceptBestRelative.get());
    acceptBestAbsoluteDoubleSpinBox->setValue(settings.acceptBestAbsolute.get());
    newRoomPenaltyDoubleSpinBox->setValue(settings.newRoomPenalty.get());
    correctPositionBonusDoubleSpinBox->setValue(settings.correctPositionBonus.get());
    maxPaths->setValue(settings.maxPaths.get());
    matchingToleranceSpinBox->setValue(settings.matchingTolerance.get());
    multipleConnectionsPenaltyDoubleSpinBox->setValue(settings.multipleConnectionsPenalty.get());
}

void PathmachinePage::slot_acceptBestRelativeDoubleSpinBoxValueChanged(const double val)
{
    setConfig().pathMachine.acceptBestRelative.set(val);
}

void PathmachinePage::slot_acceptBestAbsoluteDoubleSpinBoxValueChanged(const double val)
{
    setConfig().pathMachine.acceptBestAbsolute.set(val);
}

void PathmachinePage::slot_newRoomPenaltyDoubleSpinBoxValueChanged(const double val)
{
    setConfig().pathMachine.newRoomPenalty.set(val);
}

void PathmachinePage::slot_correctPositionBonusDoubleSpinBoxValueChanged(const double val)
{
    setConfig().pathMachine.correctPositionBonus.set(val);
}

void PathmachinePage::slot_multipleConnectionsPenaltyDoubleSpinBoxValueChanged(const double val)
{
    setConfig().pathMachine.multipleConnectionsPenalty.set(val);
}

void PathmachinePage::slot_maxPathsValueChanged(const int val)
{
    setConfig().pathMachine.maxPaths.set(utils::clampNonNegative(val));
}

void PathmachinePage::slot_matchingToleranceSpinBoxValueChanged(const int val)
{
    setConfig().pathMachine.matchingTolerance.set(utils::clampNonNegative(val));
}
