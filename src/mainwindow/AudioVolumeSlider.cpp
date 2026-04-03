// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "AudioVolumeSlider.h"

#include "../configuration/configuration.h"
#include "../global/SignalBlocker.h"

AudioVolumeSlider::AudioVolumeSlider(QWidget *const parent)
    : QSlider(Qt::Orientation::Horizontal, parent)
{
    setRange(0, 100);
    connect(this, &QSlider::valueChanged, this, [this](int value) { updateToConfig(value); });

    setConfig().audio.registerChangeCallback(m_lifetime, [this]() { updateFromConfig(); });

    if constexpr (NO_AUDIO) {
        setEnabled(false);
    }
    setAudioType(m_type);
}

AudioVolumeSlider::AudioVolumeSlider(AudioType type, QWidget *const parent)
    : AudioVolumeSlider(parent)
{
    setAudioType(type);
}

AudioVolumeSlider::~AudioVolumeSlider() = default;

void AudioVolumeSlider::setAudioType(AudioType type)
{
    m_type = type;
    updateFromConfig();

    switch (m_type) {
    case AudioType::Music:
        setToolTip(tr("Music Volume"));
        break;
    case AudioType::Sound:
        setToolTip(tr("Sound Volume"));
        break;
    }
}

void AudioVolumeSlider::updateFromConfig()
{
    int actualVolume = 0;
    switch (m_type) {
    case AudioType::Music:
        actualVolume = getConfig().audio.getMusicVolume();
        break;
    case AudioType::Sound:
        actualVolume = getConfig().audio.getSoundVolume();
        break;
    }

    if (value() != actualVolume) {
        const SignalBlocker block{*this};
        setValue(actualVolume);
    }
}

void AudioVolumeSlider::updateToConfig(int value)
{
    auto &audioSettings = setConfig().audio;
    switch (m_type) {
    case AudioType::Music:
        if (audioSettings.getMusicVolume() != value) {
            audioSettings.setMusicVolume(value);
            audioSettings.setUnlocked();
        }
        break;
    case AudioType::Sound:
        if (audioSettings.getSoundVolume() != value) {
            audioSettings.setSoundVolume(value);
            audioSettings.setUnlocked();
        }
        break;
    }
}
