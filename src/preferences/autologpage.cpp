// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Mattias Viklund <devmew@exedump.com> (Mew_)

#include "autologpage.h"

#include <QFont>
#include <QFontInfo>
#include <QPalette>
#include <QSpinBox>
#include <QString>
#include <QtGui>
#include <QtWidgets>

#include "../configuration/configuration.h"
#include "ui_autologpage.h"

AutoLogPage::AutoLogPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::AutoLogPage)
{
    ui->setupUi(this);

    connect(ui->autoLogCheckBox,
            &QCheckBox::stateChanged,
            this,
            &AutoLogPage::autoLogCheckBoxChanged);
    connect(ui->selectAutoLogLocationButton,
            &QAbstractButton::clicked,
            this,
            &AutoLogPage::selectLogLocationButtonClicked);
    connect(ui->autoLogMaxLines,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &AutoLogPage::maxLogLinesChanged);
    connect(ui->deleteOldLogsCheckbox,
            QOverload<int>::of(&QCheckBox::stateChanged),
            this,
            &AutoLogPage::deleteOldLogsCheckBoxChanged);
    connect(ui->deleteLogsOlderThan,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &AutoLogPage::deleteLogsOlderThanChanged);
    connect(ui->warnWhenDeletingCheckBox,
            &QCheckBox::stateChanged,
            this,
            &AutoLogPage::warnWhenDeletingCheckBoxChanged);
    connect(ui->warnWhenMoreThan,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &AutoLogPage::warnWhenMoreThanChanged);
}

AutoLogPage::~AutoLogPage()
{
    delete ui;
}

void AutoLogPage::loadConfig()
{
    const auto &config = getConfig();
    const auto &autoLog = config.autoLog;

    ui->autoLogCheckBox->setChecked(autoLog.autoLog);
    ui->autoLogLocation->setText(autoLog.autoLogDirectory);
    ui->autoLogMaxLines->setValue(autoLog.autoLogMaxLines);
    ui->deleteOldLogsCheckbox->setChecked(autoLog.deleteOldLogs);
    ui->deleteLogsOlderThan->setValue(autoLog.deleteLogsOlderThan);
    ui->warnWhenDeletingCheckBox->setChecked(autoLog.warnWhenDeleting);
    ui->warnWhenMoreThan->setValue(autoLog.warnWhenMoreThan);
}

void AutoLogPage::selectLogLocationButtonClicked(int /*unused*/)
{
    QString logDirectory = QFileDialog::getExistingDirectory(this,
                                                             "Choose log location ...",
                                                             QDir::currentPath());

    if (!logDirectory.isEmpty()) {
        ui->autoLogLocation->setText(logDirectory);
        auto &savedAutoLog = setConfig().autoLog;
        savedAutoLog.autoLogDirectory = logDirectory;
        savedAutoLog.autoLog = true;

        ui->autoLogCheckBox->setChecked(true);
    }
}

void AutoLogPage::autoLogCheckBoxChanged(int /*unused*/)
{
    setConfig().autoLog.autoLog = ui->autoLogCheckBox->isChecked();
}

void AutoLogPage::maxLogLinesChanged(int /*unused*/)
{
    setConfig().autoLog.autoLogMaxLines = ui->autoLogMaxLines->value();
}

void AutoLogPage::deleteOldLogsCheckBoxChanged(int /*unused*/)
{
    setConfig().autoLog.deleteOldLogs = ui->deleteOldLogsCheckbox->isChecked();
}

void AutoLogPage::deleteLogsOlderThanChanged(int /*unused*/)
{
    setConfig().autoLog.deleteLogsOlderThan = ui->deleteLogsOlderThan->value();
}

void AutoLogPage::warnWhenDeletingCheckBoxChanged(int /*unused*/)
{
    setConfig().autoLog.warnWhenDeleting = ui->warnWhenDeletingCheckBox->isChecked();
}

void AutoLogPage::warnWhenMoreThanChanged(int /*unused*/)
{
    setConfig().autoLog.warnWhenMoreThan = ui->warnWhenMoreThan->value();
}
