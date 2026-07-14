// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "AudioPageAdapter.h"

#include "../configuration/configuration.h"

#ifndef MMAPPER_NO_AUDIO
#include <QAudioDevice>
#include <QMediaDevices>
#endif

AudioPageAdapter::AudioPageAdapter(QObject *const parent)
    : QObject(parent)
{
    refreshDevices();

    setConfig().audio.registerChangeCallback(m_lifetime, [this]() { emit sig_changed(); });

#ifndef MMAPPER_NO_AUDIO
    if constexpr (!NO_AUDIO) {
        m_mediaDevices = new QMediaDevices(this);
        connect(m_mediaDevices,
                &QMediaDevices::audioOutputsChanged,
                this,
                &AudioPageAdapter::refreshDevices);
    }
#endif
}

bool AudioPageAdapter::getEnabled()
{
    return !NO_AUDIO;
}

int AudioPageAdapter::getSelectedDeviceIndex() const
{
    const QByteArray currentId = getConfig().audio.getOutputDeviceId();
    const int index = static_cast<int>(m_deviceIds.indexOf(currentId));
    return index >= 0 ? index : 0;
}

void AudioPageAdapter::setSelectedDeviceIndex(const int index)
{
    if (index < 0 || index >= m_deviceIds.size()) {
        return;
    }
    setConfig().audio.setOutputDeviceId(m_deviceIds.at(index));
    emit sig_changed();
}

int AudioPageAdapter::getMusicVolume() const
{
    return getConfig().audio.getMusicVolume();
}

void AudioPageAdapter::setMusicVolume(const int value)
{
    auto &audioSettings = setConfig().audio;
    if (audioSettings.getMusicVolume() != value) {
        audioSettings.setMusicVolume(value);
        audioSettings.setUnlocked();
    }
}

int AudioPageAdapter::getSoundVolume() const
{
    return getConfig().audio.getSoundVolume();
}

void AudioPageAdapter::setSoundVolume(const int value)
{
    auto &audioSettings = setConfig().audio;
    if (audioSettings.getSoundVolume() != value) {
        audioSettings.setSoundVolume(value);
        audioSettings.setUnlocked();
    }
}

void AudioPageAdapter::refreshDevices()
{
    QByteArray currentId = getConfig().audio.getOutputDeviceId();

    m_deviceNames.clear();
    m_deviceIds.clear();

    m_deviceNames.append(tr("System Default"));
    m_deviceIds.append(QByteArray());

#ifndef MMAPPER_NO_AUDIO
    const QList<QAudioDevice> devices = QMediaDevices::audioOutputs();
    bool found = currentId.isEmpty();
    for (const QAudioDevice &device : devices) {
        m_deviceNames.append(device.description());
        m_deviceIds.append(device.id());
        if (device.id() == currentId) {
            found = true;
        }
    }

    if (!currentId.isEmpty() && !found) {
        m_deviceNames.append(tr("%1 (missing)").arg(QString::fromUtf8(currentId)));
        m_deviceIds.append(currentId);
    }
#endif

    emit sig_devicesChanged();
    emit sig_changed();
}

void AudioPageAdapter::reload()
{
    refreshDevices();
    emit sig_changed();
}
