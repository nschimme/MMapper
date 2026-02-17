// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "SfxManager.h"

#include "AudioLibrary.h"
#include "../configuration/configuration.h"

#ifndef MMAPPER_NO_AUDIO
#include <QSoundEffect>
#endif
#include <QUrl>

SfxManager::SfxManager(const AudioLibrary &library, QObject *parent)
    : QObject(parent)
    , m_library(library)
{
}

void SfxManager::playSound([[maybe_unused]] const QString &soundName)
{
#ifndef MMAPPER_NO_AUDIO
    QString path = m_library.findAudioFile("sounds", soundName);
    if (path.isEmpty()) {
        return;
    }

    auto *effect = new QSoundEffect(this);
    if (path.startsWith(":/")) {
        effect->setSource(QUrl(QStringLiteral("qrc") + path));
    } else {
        effect->setSource(QUrl::fromLocalFile(path));
    }

    effect->setVolume(getConfig().audio.soundVolume / 100.0f);

    connect(effect, &QSoundEffect::playingChanged, this, [this, effect]() {
        if (!effect->isPlaying()) {
            m_activeEffects.remove(effect);
            effect->deleteLater();
        }
    });

    m_activeEffects.insert(effect);
    effect->play();
#endif
}

void SfxManager::updateVolume()
{
#ifndef MMAPPER_NO_AUDIO
    float volume = getConfig().audio.soundVolume / 100.0f;
    for (auto *effect : m_activeEffects) {
        effect->setVolume(volume);
    }
#endif
}
