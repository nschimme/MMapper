#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/RuleOf5.h"
#include "../global/Signal2.h"
#include "../global/macros.h"
#include "AudioVolumeSlider.h"

#include <QObject>

// QML-facing counterpart to AudioVolumeSlider (see AudioVolumeSlider.h):
// the same "read/write Configuration::audio's music/sound volume, apply-on-
// change" state, but as a plain QObject instead of a QSlider subclass, so
// Shell B's toolbar (a QQC2.Slider in MainShell.qml, not a QWidget) can bind
// against it the same way AudioVolumeSlider drives MainWindow's QToolBar
// widget. Deliberately a separate class rather than a shared base: reusing
// AudioVolumeSlider itself would mean giving it a QWidget-free code path,
// which -- since QSlider's value()/setValue()/wheelEvent() are all
// inherently QWidget-flavored -- would be more invasive than duplicating
// this ~20-line config round-trip; see the task report for this tradeoff.
class NODISCARD_QOBJECT AudioVolumeController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int volume READ getVolume WRITE setVolume NOTIFY sig_volumeChanged)

public:
    using AudioType = AudioVolumeSlider::AudioType;

private:
    Signal2Lifetime m_lifetime;
    AudioType m_type;
    int m_volume = 0;

public:
    explicit AudioVolumeController(AudioType type, QObject *parent = nullptr);
    ~AudioVolumeController() final;
    DELETE_CTORS_AND_ASSIGN_OPS(AudioVolumeController);

public:
    NODISCARD int getVolume() const { return m_volume; }
    void setVolume(int volume);

signals:
    void sig_volumeChanged(int volume);

private:
    void updateFromConfig();
};
