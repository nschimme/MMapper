// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "audiopage.h"

#include "../configuration/configuration.h"
#include "../global/SignalBlocker.h"
#include "../mainwindow/AudioVolumeSlider.h"
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

    auto *const musicVolumeSlider = new AudioVolumeSlider(AudioVolumeSlider::AudioType::Music, this);
    auto *const soundsVolumeSlider
        = new AudioVolumeSlider(AudioVolumeSlider::AudioType::Sound, this);

    ui->gridLayoutMusic->replaceWidget(ui->musicVolumeSlider, musicVolumeSlider);
    ui->gridLayoutSounds->replaceWidget(ui->soundsVolumeSlider, soundsVolumeSlider);

    delete ui->musicVolumeSlider;
    delete ui->soundsVolumeSlider;

    // Use these names to avoid breaking layout or other code if it expects them
    musicVolumeSlider->setObjectName("musicVolumeSlider");
    soundsVolumeSlider->setObjectName("soundsVolumeSlider");

    slot_loadConfig();

    connect(ui->outputDeviceComboBox,
            &QComboBox::currentIndexChanged,
            this,
            &AudioPage::slot_outputDeviceChanged);

#ifndef MMAPPER_NO_AUDIO
    auto *const mediaDevices = new QMediaDevices(this);
    connect(mediaDevices, &QMediaDevices::audioOutputsChanged, this, &AudioPage::slot_updateDevices);
#endif

    if constexpr (NO_AUDIO) {
        ui->outputDeviceComboBox->setEnabled(false);
    }
}

AudioPage::~AudioPage()
{
    delete ui;
}

void AudioPage::slot_loadConfig()
{
    SignalBlocker outputBlocker(*ui->outputDeviceComboBox);

    const auto &settings = getConfig().audio;

    int index = ui->outputDeviceComboBox->findData(settings.getOutputDeviceId());
    if (index != -1) {
        ui->outputDeviceComboBox->setCurrentIndex(index);
    } else {
        ui->outputDeviceComboBox->setCurrentIndex(0);
    }
}

void AudioPage::slot_outputDeviceChanged(int index)
{
    if (index < 0) {
        return;
    }
    auto &settings = setConfig().audio;
    settings.setOutputDeviceId(ui->outputDeviceComboBox->itemData(index).toByteArray());
}

void AudioPage::slot_updateDevices()
{
    SignalBlocker blocker(*ui->outputDeviceComboBox);
    QByteArray currentId = ui->outputDeviceComboBox->currentData().toByteArray();
    if (currentId.isEmpty()) {
        currentId = getConfig().audio.getOutputDeviceId();
    }

    ui->outputDeviceComboBox->clear();
    ui->outputDeviceComboBox->addItem(tr("System Default"), QByteArray());

#ifndef MMAPPER_NO_AUDIO
    const QList<QAudioDevice> devices = QMediaDevices::audioOutputs();
    bool found = false;
    for (const QAudioDevice &device : devices) {
        ui->outputDeviceComboBox->addItem(device.description(), device.id());
        if (device.id() == currentId) {
            found = true;
        }
    }

    if (!currentId.isEmpty() && !found) {
        ui->outputDeviceComboBox->addItem(tr("%1 (missing)").arg(QString::fromUtf8(currentId)),
                                          currentId);
    }
#endif

    int index = ui->outputDeviceComboBox->findData(currentId);
    if (index != -1) {
        ui->outputDeviceComboBox->setCurrentIndex(index);
    } else {
        ui->outputDeviceComboBox->setCurrentIndex(0);
    }
}
