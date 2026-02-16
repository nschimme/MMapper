// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "audiopage.h"

#include "../configuration/configuration.h"
#include "ui_audiopage.h"
AudioPage::AudioPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::AudioPage)
{
    ui->setupUi(this);

    slot_loadConfig();

#ifndef MMAPPER_NO_AUDIO
    connect(ui->musicVolumeSlider,
            &QSlider::valueChanged,
            this,
            &AudioPage::slot_musicVolumeChanged);
    connect(ui->soundsVolumeSlider,
            &QSlider::valueChanged,
            this,
            &AudioPage::slot_soundsVolumeChanged);
#else
    ui->musicGroupBox->setEnabled(false);
    ui->soundsGroupBox->setEnabled(false);
    ui->musicVolumeSlider->setEnabled(false);
    ui->soundsVolumeSlider->setEnabled(false);
#endif
}

AudioPage::~AudioPage()
{
    delete ui;
}

void AudioPage::slot_loadConfig()
{
    const auto &settings = getConfig().audio;
    ui->musicVolumeSlider->setValue(settings.musicVolume);
    ui->soundsVolumeSlider->setValue(settings.soundVolume);
}

void AudioPage::slot_musicVolumeChanged(int value)
{
    setConfig().audio.musicVolume = value;
    emit sig_audioSettingsChanged();
}

void AudioPage::slot_soundsVolumeChanged(int value)
{
    setConfig().audio.soundVolume = value;
    emit sig_audioSettingsChanged();
}
