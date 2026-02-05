// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "mumeclockwidget.h"
#include "ui_mumeclockwidget.h"

MumeClockWidget::MumeClockWidget(QWidget *parent)
    : QWidget(parent), ui(new Ui::MumeClockWidget)
{
    ui->setupUi(this);
    connect(&m_viewModel, &MumeClockViewModel::clockChanged, this, &MumeClockWidget::updateUI);
    updateUI();
}

MumeClockWidget::~MumeClockWidget() = default;

void MumeClockWidget::updateUI() {
    ui->timeLabel->setText(m_viewModel.timeString());
    ui->seasonLabel->setText(m_viewModel.seasonString());
}
