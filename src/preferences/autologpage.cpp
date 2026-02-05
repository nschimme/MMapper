// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "autologpage.h"
#include "ui_autologpage.h"
#include "../global/SignalBlocker.h"

AutoLogPage::AutoLogPage(QWidget *parent)
    : QWidget(parent), ui(new Ui::AutoLogPage)
{
    ui->setupUi(this);
    connect(&m_viewModel, &AutoLogPageViewModel::settingsChanged, this, &AutoLogPage::updateUI);

    connect(ui->autoLogLocation, &QLineEdit::textChanged, &m_viewModel, &AutoLogPageViewModel::setAutoLogDirectory);
    connect(ui->autoLogCheckBox, &QCheckBox::toggled, &m_viewModel, &AutoLogPageViewModel::setAutoLog);
    connect(ui->askDeleteCheckBox, &QCheckBox::toggled, &m_viewModel, &AutoLogPageViewModel::setAskDelete);
    connect(ui->spinBoxDays, QOverload<int>::of(&QSpinBox::valueChanged), &m_viewModel, &AutoLogPageViewModel::setDeleteWhenLogsReachDays);
    connect(ui->spinBoxSize, QOverload<int>::of(&QSpinBox::valueChanged), &m_viewModel, &AutoLogPageViewModel::setDeleteWhenLogsReachBytes);
    connect(ui->autoLogMaxBytes, QOverload<int>::of(&QSpinBox::valueChanged), &m_viewModel, &AutoLogPageViewModel::setRotateWhenLogsReachBytes);

    updateUI();
}

AutoLogPage::~AutoLogPage() = default;

void AutoLogPage::updateUI()
{
    SignalBlocker sb1(*ui->autoLogLocation);
    SignalBlocker sb2(*ui->autoLogCheckBox);
    SignalBlocker sb3(*ui->askDeleteCheckBox);
    SignalBlocker sb4(*ui->spinBoxDays);
    SignalBlocker sb5(*ui->spinBoxSize);
    SignalBlocker sb6(*ui->autoLogMaxBytes);

    ui->autoLogLocation->setText(m_viewModel.autoLogDirectory());
    ui->autoLogCheckBox->setChecked(m_viewModel.autoLog());
    ui->askDeleteCheckBox->setChecked(m_viewModel.askDelete());
    ui->spinBoxDays->setValue(m_viewModel.deleteWhenLogsReachDays());
    ui->spinBoxSize->setValue(m_viewModel.deleteWhenLogsReachBytes());
    ui->autoLogMaxBytes->setValue(m_viewModel.rotateWhenLogsReachBytes());
}

void AutoLogPage::slot_loadConfig()
{
    m_viewModel.loadConfig();
}
