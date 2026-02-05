// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "pathmachinepage.h"
#include "ui_pathmachinepage.h"
#include "../global/SignalBlocker.h"

PathmachinePage::PathmachinePage(QWidget *parent)
    : QWidget(parent), ui(new Ui::PathmachinePage)
{
    ui->setupUi(this);
    connect(&m_viewModel, &PathMachinePageViewModel::settingsChanged, this, &PathmachinePage::updateUI);

    connect(ui->acceptBestRelativeDoubleSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &m_viewModel, &PathMachinePageViewModel::setAcceptBestRelative);
    connect(ui->acceptBestAbsoluteDoubleSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &m_viewModel, &PathMachinePageViewModel::setAcceptBestAbsolute);
    connect(ui->newRoomPenaltyDoubleSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &m_viewModel, &PathMachinePageViewModel::setNewRoomPenalty);
    connect(ui->multipleConnectionsPenaltyDoubleSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &m_viewModel, &PathMachinePageViewModel::setMultipleConnectionsPenalty);
    connect(ui->correctPositionBonusDoubleSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &m_viewModel, &PathMachinePageViewModel::setCorrectPositionBonus);
    connect(ui->maxPaths, QOverload<int>::of(&QSpinBox::valueChanged), &m_viewModel, &PathMachinePageViewModel::setMaxPaths);
    connect(ui->matchingToleranceSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), &m_viewModel, &PathMachinePageViewModel::setMatchingTolerance);

    updateUI();
}

PathmachinePage::~PathmachinePage() = default;

void PathmachinePage::updateUI()
{
    SignalBlocker sb1(*ui->acceptBestRelativeDoubleSpinBox);
    SignalBlocker sb2(*ui->acceptBestAbsoluteDoubleSpinBox);
    SignalBlocker sb3(*ui->newRoomPenaltyDoubleSpinBox);
    SignalBlocker sb4(*ui->multipleConnectionsPenaltyDoubleSpinBox);
    SignalBlocker sb5(*ui->correctPositionBonusDoubleSpinBox);
    SignalBlocker sb6(*ui->maxPaths);
    SignalBlocker sb7(*ui->matchingToleranceSpinBox);

    ui->acceptBestRelativeDoubleSpinBox->setValue(m_viewModel.acceptBestRelative());
    ui->acceptBestAbsoluteDoubleSpinBox->setValue(m_viewModel.acceptBestAbsolute());
    ui->newRoomPenaltyDoubleSpinBox->setValue(m_viewModel.newRoomPenalty());
    ui->multipleConnectionsPenaltyDoubleSpinBox->setValue(m_viewModel.multipleConnectionsPenalty());
    ui->correctPositionBonusDoubleSpinBox->setValue(m_viewModel.correctPositionBonus());
    ui->maxPaths->setValue(m_viewModel.maxPaths());
    ui->matchingToleranceSpinBox->setValue(m_viewModel.matchingTolerance());
}

void PathmachinePage::slot_loadConfig()
{
    m_viewModel.loadConfig();
}
