#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include <QObject>
#include <QSet>
#include <QString>

class AudioLibrary;
#ifndef MMAPPER_NO_AUDIO
class QSoundEffect;
#endif

class SfxManager final : public QObject
{
    Q_OBJECT

private:
#ifndef MMAPPER_NO_AUDIO
    QSet<QSoundEffect *> m_activeEffects;
#endif
    const AudioLibrary &m_library;

public:
    explicit SfxManager(const AudioLibrary &library, QObject *parent = nullptr);

    void playSound(const QString &soundName);
    void updateVolume();
};
