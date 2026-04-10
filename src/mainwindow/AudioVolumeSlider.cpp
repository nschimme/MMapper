// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "AudioVolumeSlider.h"

#include "../configuration/configuration.h"
#include "../global/SignalBlocker.h"

#include <QWheelEvent>

AudioVolumeSlider::AudioVolumeSlider(QWidget *const parent)
    : QSlider(Qt::Orientation::Horizontal, parent)
{
    init();
}

AudioVolumeSlider::AudioVolumeSlider(AudioType type, QWidget *const parent)
    : QSlider(Qt::Orientation::Horizontal, parent)
    , m_type(type)
{
    init();
}

AudioVolumeSlider::~AudioVolumeSlider() = default;

void AudioVolumeSlider::init()
{
    setRange(0, 100);
    connect(this, &QSlider::valueChanged, this, [this](int value) { updateToConfig(value); });

    if constexpr (NO_AUDIO) {
        setEnabled(false);
    }
    setAudioType(m_type);
}

void AudioVolumeSlider::setConfiguration(Configuration &config)
{
    m_config = &config;
    m_config->audio.registerChangeCallback(m_lifetime, [this]() { updateFromConfig(); });
    updateFromConfig();
}

void AudioVolumeSlider::setAudioType(AudioType type)
{
    if (m_type == type && toolTip().length() > 0) {
        return;
    }

    m_type = type;

    if (m_config == nullptr) {
        setConfig().audio.registerChangeCallback(m_lifetime, [this]() { updateFromConfig(); });
    }

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
    const auto &audio = m_config ? m_config->audio : getConfig().audio;
    switch (m_type) {
    case AudioType::Music:
        actualVolume = audio.getMusicVolume();
        break;
    case AudioType::Sound:
        actualVolume = audio.getSoundVolume();
        break;
    }

    if (value() != actualVolume) {
        const SignalBlocker block{*this};
        setValue(actualVolume);
    }
}

void AudioVolumeSlider::updateToConfig(int value)
{
    auto &audioSettings = m_config ? m_config->audio : setConfig().audio;
    bool changed = false;
    switch (m_type) {
    case AudioType::Music:
        if (audioSettings.getMusicVolume() != value) {
            audioSettings.setMusicVolume(value);
            audioSettings.setUnlocked();
            changed = true;
        }
        break;
    case AudioType::Sound:
        if (audioSettings.getSoundVolume() != value) {
            audioSettings.setSoundVolume(value);
            audioSettings.setUnlocked();
            changed = true;
        }
        break;
    }
    if (changed) {
    }
}

void AudioVolumeSlider::wheelEvent(QWheelEvent *event)
{
    event->ignore();
}
