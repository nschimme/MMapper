// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "maintenancepage.h"
#include "ui_maintenancepage.h"

#include "../configuration/configuration.h"
#include "../mainwindow/mainwindow.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>

MaintenancePage::MaintenancePage(MainWindow *mainWindow, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MaintenancePage)
    , m_mainWindow(mainWindow)
{
    ui->setupUi(this);

    connect(ui->exportButton, &QPushButton::clicked, this, &MaintenancePage::slot_exportConfiguration);
    connect(ui->importButton, &QPushButton::clicked, this, &MaintenancePage::slot_importConfiguration);
    connect(ui->resetButton, &QPushButton::clicked, this, &MaintenancePage::slot_factoryReset);
}

MaintenancePage::~MaintenancePage()
{
    delete ui;
}

void MaintenancePage::slot_loadConfig()
{
    // Nothing to load currently, as there are no configurable fields on this page yet.
}

void MaintenancePage::slot_exportConfiguration()
{
    QString fileName = QFileDialog::getSaveFileName(this,
                                                    tr("Export Configuration"),
                                                    QString(),
                                                    tr("Configuration Files (*.ini)"));
    if (fileName.isEmpty()) {
        return;
    }

    QSettings settings(fileName, QSettings::IniFormat);
    getConfig().writeTo(settings);
    settings.sync();

    QMessageBox::information(this, tr("Export Configuration"), tr("Configuration exported successfully to %1").arg(fileName));
}

void MaintenancePage::slot_importConfiguration()
{
    if (m_mainWindow && m_mainWindow->isConnected()) {
        QMessageBox::warning(this, tr("Import Configuration"), tr("You must disconnect before you can reload the configuration."));
        return;
    }

    QString fileName = QFileDialog::getOpenFileName(this,
                                                    tr("Import Configuration"),
                                                    QString(),
                                                    tr("Configuration Files (*.ini)"));
    if (fileName.isEmpty()) {
        return;
    }

    if (QMessageBox::question(this, tr("Import Configuration"),
                              tr("Are you sure you want to import the configuration from %1? This will overwrite your current settings.").arg(fileName),
                              QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
        return;
    }

    QSettings settings(fileName, QSettings::IniFormat);
    setConfig().readFrom(settings);
    setConfig().write();

    emit sig_loadConfig();
    QMessageBox::information(this, tr("Import Configuration"), tr("Configuration imported successfully."));
}

void MaintenancePage::slot_factoryReset()
{
    if (m_mainWindow && m_mainWindow->isConnected()) {
        QMessageBox::warning(this, tr("Factory Reset"), tr("You must disconnect before you can do a factory reset."));
        return;
    }

    QMessageBox::StandardButton reply
        = QMessageBox::question(this,
                                "MMapper Factory Reset",
                                "Are you sure you want to perform a factory reset?",
                                QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        setConfig().reset();
        emit sig_factoryReset();
        QMessageBox::information(this, tr("Factory Reset"), tr("Configuration has been reset to defaults."));
    }
}
