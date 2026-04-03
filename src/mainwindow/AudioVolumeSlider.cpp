// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "AudioVolumeSlider.h"

#include "../configuration/configuration.h"
#include "../global/SignalBlocker.h"

AudioVolumeSlider::AudioVolumeSlider(AudioType type, QWidget *const parent)
    : QSlider(Qt::Orientation::Horizontal, parent)
    , m_type(type)
{
    setRange(0, 100);
    updateFromConfig();

    connect(this, &QSlider::valueChanged, this, [this](int value) { updateToConfig(value); });

    setConfig().audio.registerChangeCallback(m_lifetime, [this]() { updateFromConfig(); });

    switch (m_type) {
    case AudioType::Music:
        setToolTip(tr("Music Volume"));
        break;
    case AudioType::Sound:
        setToolTip(tr("Sound Volume"));
        break;
    }
}

AudioVolumeSlider::~AudioVolumeSlider() = default;

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
    switch (m_type) {
    case AudioType::Music:
        if (getConfig().audio.getMusicVolume() != value) {
            setConfig().audio.setMusicVolume(value);
        }
        break;
    case AudioType::Sound:
        if (getConfig().audio.getSoundVolume() != value) {
            setConfig().audio.setSoundVolume(value);
        }
        break;
    }
}
