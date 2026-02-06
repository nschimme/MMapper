// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "mumeclockwidget.h"
#include "ui_mumeclockwidget.h"
#include "mumeclock.h"
#include "../observer/gameobserver.h"

MumeClockWidget::MumeClockWidget(GameObserver &observer, MumeClock &clock, QWidget *parent)
    : QWidget(parent), ui(new Ui::MumeClockWidget)
{
    ui->setupUi(this);

    auto updateFunc = [this, &clock]() {
        const auto moment = clock.getMumeMoment();
        m_viewModel.update(clock.toMumeTime(moment), ""); // simplified season for now
    };

    connect(&clock, &MumeClock::slot_tick, this, updateFunc);
    connect(&m_viewModel, &MumeClockViewModel::clockChanged, this, &MumeClockWidget::updateUI);

    updateUI();
}

MumeClockWidget::~MumeClockWidget() = default;

void MumeClockWidget::updateUI() {
    ui->timeLabel->setText(m_viewModel.timeString());
    ui->seasonLabel->setText(m_viewModel.seasonString());
}
