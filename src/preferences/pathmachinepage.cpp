// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "pathmachinepage.h"

#include "../configuration/configuration.h"

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
    connect(onlyAllowChangesInMapModeCheckBox,
            &QCheckBox::stateChanged,
            this,
            &PathmachinePage::slot_onlyAllowChangesInMapModeCheckBoxStateChanged);
}

void PathmachinePage::slot_loadConfig()
{
    const auto &settings = getConfig().pathMachine;
    acceptBestRelativeDoubleSpinBox->setValue(settings.acceptBestRelative.get());
    acceptBestAbsoluteDoubleSpinBox->setValue(settings.acceptBestAbsolute.get());
    newRoomPenaltyDoubleSpinBox->setValue(settings.newRoomPenalty.get());
    correctPositionBonusDoubleSpinBox->setValue(settings.correctPositionBonus.get());
    maxPaths->setValue(settings.maxPaths.get());
    matchingToleranceSpinBox->setValue(settings.matchingTolerance.get());
    multipleConnectionsPenaltyDoubleSpinBox->setValue(settings.multipleConnectionsPenalty.get());
    onlyAllowChangesInMapModeCheckBox->setChecked(settings.onlyAllowChangesInMapMode.get());
    // TODO: Add UI element for maxSkipped and load it here
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

void PathmachinePage::slot_onlyAllowChangesInMapModeCheckBoxStateChanged(const int state)
{
    setConfig().pathMachine.onlyAllowChangesInMapMode.set(state == Qt::Checked);
}
// TODO: Add slot for maxSkipped when UI element is added
