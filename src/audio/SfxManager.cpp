// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "SfxManager.h"

#include "../configuration/configuration.h"
#include "AudioLibrary.h"

#ifndef MMAPPER_NO_AUDIO
#include <QSoundEffect>
#include <QUrl>
#endif

SfxManager::SfxManager(const AudioLibrary &library, QObject *parent)
    : QObject(parent)
    , m_library(library)
{}

void SfxManager::playSound(const QString &soundName)
{
#ifndef MMAPPER_NO_AUDIO
    if (getConfig().audio.soundVolume <= 0) {
        return;
    }

    QString name = soundName.toLower().replace(' ', '-');
    QString soundFile = m_library.findAudioFile("sounds", name);
    if (soundFile.isEmpty()) {
        return;
    }

    QSoundEffect *effect = new QSoundEffect(this);
    if (soundFile.startsWith(":")) {
        effect->setSource(QUrl("qrc" + soundFile));
    } else {
        effect->setSource(QUrl::fromLocalFile(soundFile));
    }
    effect->setVolume(static_cast<float>(getConfig().audio.soundVolume) / 100.0f);

    connect(effect, &QSoundEffect::loadedChanged, this, [effect]() {
        if (effect->isLoaded()) {
            effect->play();
        }
    });

    // Cleanup when done
    connect(effect, &QSoundEffect::playingChanged, this, [effect]() {
        if (!effect->isPlaying()) {
            effect->deleteLater();
        }
    });
#else
    Q_UNUSED(soundName);
#endif
}

void SfxManager::updateVolume()
{
    // Individual sound effects are updated at play time,
    // but we could track active ones if needed.
}
