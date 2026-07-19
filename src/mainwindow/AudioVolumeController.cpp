// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "AudioVolumeController.h"

#include "../configuration/configuration.h"

AudioVolumeController::AudioVolumeController(const AudioType type, QObject *const parent)
    : QObject(parent)
    , m_type(type)
{
    setConfig().audio.registerChangeCallback(m_lifetime, [this]() { updateFromConfig(); });
    updateFromConfig();
}

AudioVolumeController::~AudioVolumeController() = default;

void AudioVolumeController::setVolume(const int volume)
{
    if (m_volume == volume) {
        return;
    }
    m_volume = volume;

    auto &audioSettings = setConfig().audio;
    switch (m_type) {
    case AudioType::Music:
        if (audioSettings.getMusicVolume() != volume) {
            audioSettings.setMusicVolume(volume);
            audioSettings.setUnlocked();
        }
        break;
    case AudioType::Sound:
        if (audioSettings.getSoundVolume() != volume) {
            audioSettings.setSoundVolume(volume);
            audioSettings.setUnlocked();
        }
        break;
    }

    emit sig_volumeChanged(volume);
}

void AudioVolumeController::updateFromConfig()
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

    if (m_volume != actualVolume) {
        m_volume = actualVolume;
        emit sig_volumeChanged(actualVolume);
    }
}
