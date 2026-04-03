#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "../global/Signal2.h"
#include "../global/macros.h"

#include <QSlider>

class AudioVolumeSlider final : public QSlider
{
    Q_OBJECT

public:
    enum class AudioType { Music, Sound };

private:
    AudioType m_type;
    Signal2Lifetime m_lifetime;

public:
    explicit AudioVolumeSlider(AudioType type, QWidget *parent = nullptr);
    ~AudioVolumeSlider() final;

private:
    void updateFromConfig();
    void updateToConfig(int value);
};
