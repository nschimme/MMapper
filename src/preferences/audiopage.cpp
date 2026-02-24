// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "audiopage.h"

#include "../configuration/configuration.h"
#include "ui_audiopage.h"

#ifndef MMAPPER_NO_AUDIO
#include <QAudioDevice>
#include <QMediaDevices>
#endif

AudioPage::AudioPage(QWidget *const parent)
    : QWidget(parent)
    , ui(new Ui::AudioPage)
{
    ui->setupUi(this);

    slot_updateDevices();
    slot_loadConfig();

    connect(ui->musicVolumeSlider,
            &QSlider::valueChanged,
            this,
            &AudioPage::slot_musicVolumeChanged);
    connect(ui->soundsVolumeSlider,
            &QSlider::valueChanged,
            this,
            &AudioPage::slot_soundsVolumeChanged);
    connect(ui->outputDeviceComboBox,
            &QComboBox::currentIndexChanged,
            this,
            &AudioPage::slot_outputDeviceChanged);

#ifndef MMAPPER_NO_AUDIO
    auto *const mediaDevices = new QMediaDevices(this);
    connect(mediaDevices,
            &QMediaDevices::audioOutputsChanged,
            this,
            &AudioPage::slot_updateDevices);
#endif

    if constexpr (NO_AUDIO) {
        ui->outputDeviceComboBox->setEnabled(false);
        ui->musicVolumeSlider->setEnabled(false);
        ui->soundsVolumeSlider->setEnabled(false);
    }
}

AudioPage::~AudioPage()
{
    delete ui;
}

void AudioPage::slot_loadConfig()
{
    const auto &settings = getConfig().audio;
    ui->musicVolumeSlider->setValue(settings.getMusicVolume());
    ui->soundsVolumeSlider->setValue(settings.getSoundVolume());

    int index = ui->outputDeviceComboBox->findData(settings.getOutputDeviceId());
    if (index != -1) {
        ui->outputDeviceComboBox->setCurrentIndex(index);
    } else {
        ui->outputDeviceComboBox->setCurrentIndex(0);
    }
}

void AudioPage::slot_musicVolumeChanged(int value)
{
    auto &settings = setConfig().audio;
    settings.setMusicVolume(value);
    settings.setUnlocked();
}

void AudioPage::slot_soundsVolumeChanged(int value)
{
    auto &settings = setConfig().audio;
    settings.setSoundVolume(value);
    settings.setUnlocked();
}

void AudioPage::slot_outputDeviceChanged(int index)
{
    if (index < 0) {
        return;
    }
    auto &settings = setConfig().audio;
    settings.setOutputDeviceId(ui->outputDeviceComboBox->itemData(index).toString());
}

void AudioPage::slot_updateDevices()
{
    ui->outputDeviceComboBox->blockSignals(true);
    QString currentId = ui->outputDeviceComboBox->currentData().toString();
    if (currentId.isEmpty()) {
        currentId = getConfig().audio.getOutputDeviceId();
    }

    ui->outputDeviceComboBox->clear();
    ui->outputDeviceComboBox->addItem(tr("System Default"), QString());

#ifndef MMAPPER_NO_AUDIO
    const QList<QAudioDevice> devices = QMediaDevices::audioOutputs();
    for (const QAudioDevice &device : devices) {
        ui->outputDeviceComboBox->addItem(device.description(), device.id());
    }
#endif

    int index = ui->outputDeviceComboBox->findData(currentId);
    if (index != -1) {
        ui->outputDeviceComboBox->setCurrentIndex(index);
    } else {
        ui->outputDeviceComboBox->setCurrentIndex(0);
    }
    ui->outputDeviceComboBox->blockSignals(false);
}
