// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Mattias 'Mew_' Viklund <devmew@exedump.com> (Mirnir)

#include "autologpage.h"

#include "../configuration/configuration.h"
#include "../global/SignalBlocker.h"
#include "ui_autologpage.h"

#include <QSpinBox>
#include <QString>
#include <QtGui>
#include <QtWidgets>

static const constexpr int MEGABYTE_IN_BYTES = 1000000;

AutoLogPage::AutoLogPage(QWidget *const parent)
    : QWidget(parent)
    , ui(new Ui::AutoLogPage)
{
    ui->setupUi(this);

    connect(ui->autoLogCheckBox,
            QOverload<bool>::of(&QCheckBox::toggled),
            this,
            [](const bool autoLog) { setConfig().autoLog.autoLog.set(autoLog); });
    connect(ui->selectAutoLogLocationButton,
            &QAbstractButton::clicked,
            this,
            &AutoLogPage::slot_selectLogLocationButtonClicked);

    connect(ui->radioButtonKeepForever,
            QOverload<bool>::of(&QRadioButton::toggled),
            this,
            &AutoLogPage::slot_logStrategyChanged);
    connect(ui->radioButtonDeleteDays,
            QOverload<bool>::of(&QRadioButton::toggled),
            this,
            &AutoLogPage::slot_logStrategyChanged);
    connect(ui->spinBoxDays, QOverload<int>::of(&QSpinBox::valueChanged), this, [](const int size) {
        setConfig().autoLog.deleteWhenLogsReachDays.set(size);
    });
    connect(ui->radioButtonDeleteSize,
            QOverload<bool>::of(&QRadioButton::toggled),
            this,
            &AutoLogPage::slot_logStrategyChanged);
    connect(ui->spinBoxSize, QOverload<int>::of(&QSpinBox::valueChanged), this, [](const int size) {
        setConfig().autoLog.deleteWhenLogsReachBytes.set(size * MEGABYTE_IN_BYTES);
    });
    connect(ui->askDeleteCheckBox,
            QOverload<bool>::of(&QCheckBox::toggled),
            this,
            [](const bool askDelete) { setConfig().autoLog.askDelete.set(askDelete); });

    connect(ui->autoLogMaxBytes,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            [](const int size) {
                setConfig().autoLog.rotateWhenLogsReachBytes.set(size * MEGABYTE_IN_BYTES);
            });

    if constexpr (CURRENT_PLATFORM == PlatformEnum::Wasm) {
        ui->autoLogCheckBox->setDisabled(true);
        ui->autoLogLocation->setDisabled(true);
        ui->selectAutoLogLocationButton->setDisabled(true);
        ui->radioButtonKeepForever->setDisabled(true);
        ui->radioButtonDeleteDays->setDisabled(true);
        ui->spinBoxDays->setDisabled(true);
        ui->radioButtonDeleteSize->setDisabled(true);
        ui->spinBoxSize->setDisabled(true);
        ui->askDeleteCheckBox->setDisabled(true);
        ui->autoLogMaxBytes->setDisabled(true);
    }

    auto &autoLog = setConfig().autoLog;
    autoLog.autoLog.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
    autoLog.autoLogDirectory.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
    autoLog.cleanupStrategy.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
    autoLog.deleteWhenLogsReachDays.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
    autoLog.deleteWhenLogsReachBytes.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
    autoLog.askDelete.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
    autoLog.rotateWhenLogsReachBytes.registerChangeCallback(m_lifetime, [this]() { slot_loadConfig(); });
}

AutoLogPage::~AutoLogPage()
{
    delete ui;
}

void AutoLogPage::slot_loadConfig()
{
    const auto &config = getConfig().autoLog;

    SignalBlocker b1(*ui->autoLogCheckBox);
    SignalBlocker b2(*ui->radioButtonKeepForever);
    SignalBlocker b3(*ui->radioButtonDeleteDays);
    SignalBlocker b4(*ui->radioButtonDeleteSize);
    SignalBlocker b5(*ui->spinBoxDays);
    SignalBlocker b6(*ui->spinBoxSize);
    SignalBlocker b7(*ui->askDeleteCheckBox);
    SignalBlocker b8(*ui->autoLogMaxBytes);

    ui->autoLogCheckBox->setChecked(config.autoLog.get());
    ui->autoLogLocation->setText(config.autoLogDirectory.get());
    ui->autoLogMaxBytes->setValue(config.rotateWhenLogsReachBytes.get() / MEGABYTE_IN_BYTES);
    switch (static_cast<AutoLoggerEnum>(config.cleanupStrategy.get())) {
    case AutoLoggerEnum::KeepForever:
        ui->radioButtonKeepForever->setChecked(true);
        break;
    case AutoLoggerEnum::DeleteDays:
        ui->radioButtonDeleteDays->setChecked(true);
        break;
    case AutoLoggerEnum::DeleteSize:
        ui->radioButtonDeleteSize->setChecked(true);
        break;
    default:
        abort();
    }
    ui->spinBoxDays->setValue(config.deleteWhenLogsReachDays.get());
    ui->spinBoxSize->setValue(config.deleteWhenLogsReachBytes.get() / MEGABYTE_IN_BYTES);
    ui->askDeleteCheckBox->setChecked(config.askDelete.get());
}

void AutoLogPage::slot_selectLogLocationButtonClicked(int /*unused*/)
{
    auto &config = setConfig().autoLog;
    QString logDirectory = QFileDialog::getExistingDirectory(this,
                                                             "Choose log location ...",
                                                             config.autoLogDirectory.get());

    if (!logDirectory.isEmpty()) {
        ui->autoLogLocation->setText(logDirectory);
        config.autoLogDirectory.set(logDirectory);
    }
}

void AutoLogPage::slot_logStrategyChanged(int /*unused*/)
{
    auto &strategy = setConfig().autoLog.cleanupStrategy;
    if (ui->radioButtonKeepForever->isChecked()) {
        strategy.set(static_cast<int>(AutoLoggerEnum::KeepForever));
    } else if (ui->radioButtonDeleteDays->isChecked()) {
        strategy.set(static_cast<int>(AutoLoggerEnum::DeleteDays));
    } else if (ui->radioButtonDeleteSize->isChecked()) {
        strategy.set(static_cast<int>(AutoLoggerEnum::DeleteSize));
    } else {
        // can happen when toggling
    }
}
