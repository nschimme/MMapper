#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "../global/Signal2.h"
#include "../global/macros.h"

#include <QSlider>

class AudioVolumeSlider final : public QSlider
{
    Q_OBJECT
    Q_PROPERTY(AudioType audioType READ audioType WRITE setAudioType)

public:
    enum AudioType { Music, Sound };
    Q_ENUM(AudioType)

private:
    AudioType m_type = AudioType::Music;
    Signal2Lifetime m_lifetime;

public:
    explicit AudioVolumeSlider(QWidget *parent = nullptr);
    explicit AudioVolumeSlider(AudioType type, QWidget *parent = nullptr);
    ~AudioVolumeSlider() final;

public:
    NODISCARD AudioType audioType() const { return m_type; }
    void setAudioType(AudioType type);

private:
    void updateFromConfig();
    void updateToConfig(int value);
};
