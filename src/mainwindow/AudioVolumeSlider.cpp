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

    setToolTip(m_type == AudioType::Music ? tr("Music Volume") : tr("Sound Volume"));
}

AudioVolumeSlider::~AudioVolumeSlider() = default;

void AudioVolumeSlider::updateFromConfig()
{
    const int actualVolume = (m_type == AudioType::Music) ? getConfig().audio.getMusicVolume()
                                                          : getConfig().audio.getSoundVolume();
    if (value() != actualVolume) {
        const SignalBlocker block{*this};
        setValue(actualVolume);
    }
}

void AudioVolumeSlider::updateToConfig(int value)
{
    if (m_type == AudioType::Music) {
        if (getConfig().audio.getMusicVolume() != value) {
            setConfig().audio.setMusicVolume(value);
        }
    } else {
        if (getConfig().audio.getSoundVolume() != value) {
            setConfig().audio.setSoundVolume(value);
        }
    }
}
