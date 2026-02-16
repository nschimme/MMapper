// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "audiopage.h"

#include "../configuration/configuration.h"
#include "ui_audiopage.h"
#ifndef WITH_AUDIO
#include <QLabel>
#include <QVBoxLayout>
#endif

AudioPage::AudioPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::AudioPage)
{
    ui->setupUi(this);

#ifdef WITH_AUDIO
    slot_loadConfig();

    connect(ui->musicEnabledCheckBox,
            &QCheckBox::stateChanged,
            this,
            &AudioPage::slot_musicEnabledChanged);
    connect(ui->musicVolumeSlider,
            &QSlider::valueChanged,
            this,
            &AudioPage::slot_musicVolumeChanged);
    connect(ui->soundsEnabledCheckBox,
            &QCheckBox::stateChanged,
            this,
            &AudioPage::slot_soundsEnabledChanged);
    connect(ui->soundsVolumeSlider,
            &QSlider::valueChanged,
            this,
            &AudioPage::slot_soundsVolumeChanged);
#else
    ui->musicGroupBox->setEnabled(false);
    ui->soundsGroupBox->setEnabled(false);
    auto *label = new QLabel(
        tr("Audio features are disabled because Qt6 Multimedia was not found during build."), this);
    label->setWordWrap(true);
    ui->verticalLayout->insertWidget(0, label);
#endif
}

AudioPage::~AudioPage()
{
    delete ui;
}

void AudioPage::slot_loadConfig()
{
    const auto &settings = getConfig().audio;
    ui->musicEnabledCheckBox->setChecked(settings.musicEnabled);
    ui->musicVolumeSlider->setValue(settings.musicVolume);
    ui->soundsEnabledCheckBox->setChecked(settings.soundsEnabled);
    ui->soundsVolumeSlider->setValue(settings.soundVolume);
}

void AudioPage::slot_musicEnabledChanged(int state)
{
    setConfig().audio.musicEnabled = (state == Qt::Checked);
    emit sig_audioSettingsChanged();
}

void AudioPage::slot_musicVolumeChanged(int value)
{
    setConfig().audio.musicVolume = value;
    emit sig_audioSettingsChanged();
}

void AudioPage::slot_soundsEnabledChanged(int state)
{
    setConfig().audio.soundsEnabled = (state == Qt::Checked);
    emit sig_audioSettingsChanged();
}

void AudioPage::slot_soundsVolumeChanged(int value)
{
    setConfig().audio.soundVolume = value;
    emit sig_audioSettingsChanged();
}
