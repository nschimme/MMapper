#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"

#include <QObject>
#include <QSet>
#include <QString>

class AudioLibrary;
class QSoundEffect;

class NODISCARD_QOBJECT SfxManager final : public QObject
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
