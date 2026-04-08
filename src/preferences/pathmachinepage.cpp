// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "pathmachinepage.h"

#include "../configuration/configuration.h"

#include <QDoubleSpinBox>

PathmachinePage::PathmachinePage(QWidget *parent, Configuration &config)
    : QWidget(parent)
    , m_config(config)
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
}

void PathmachinePage::slot_loadConfig()
{
    const auto &settings = m_config.pathMachine;
    acceptBestRelativeDoubleSpinBox->setValue(settings.acceptBestRelative);
    acceptBestAbsoluteDoubleSpinBox->setValue(settings.acceptBestAbsolute);
    newRoomPenaltyDoubleSpinBox->setValue(settings.newRoomPenalty);
    correctPositionBonusDoubleSpinBox->setValue(settings.correctPositionBonus);
    maxPaths->setValue(settings.maxPaths);
    matchingToleranceSpinBox->setValue(settings.matchingTolerance);
    multipleConnectionsPenaltyDoubleSpinBox->setValue(settings.multipleConnectionsPenalty);
}

void PathmachinePage::slot_acceptBestRelativeDoubleSpinBoxValueChanged(const double val)
{
    m_config.pathMachine.acceptBestRelative = val;
    emit sig_changed();
}

void PathmachinePage::slot_acceptBestAbsoluteDoubleSpinBoxValueChanged(const double val)
{
    m_config.pathMachine.acceptBestAbsolute = val;
    emit sig_changed();
}

void PathmachinePage::slot_newRoomPenaltyDoubleSpinBoxValueChanged(const double val)
{
    m_config.pathMachine.newRoomPenalty = val;
    emit sig_changed();
}

void PathmachinePage::slot_correctPositionBonusDoubleSpinBoxValueChanged(const double val)
{
    m_config.pathMachine.correctPositionBonus = val;
    emit sig_changed();
}

void PathmachinePage::slot_multipleConnectionsPenaltyDoubleSpinBoxValueChanged(const double val)
{
    m_config.pathMachine.multipleConnectionsPenalty = val;
    emit sig_changed();
}

void PathmachinePage::slot_maxPathsValueChanged(const int val)
{
    m_config.pathMachine.maxPaths = utils::clampNonNegative(val);
    emit sig_changed();
}

void PathmachinePage::slot_matchingToleranceSpinBoxValueChanged(const int val)
{
    m_config.pathMachine.matchingTolerance = utils::clampNonNegative(val);
    emit sig_changed();
}
