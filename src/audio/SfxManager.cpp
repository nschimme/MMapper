// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "SfxManager.h"

#include "../configuration/configuration.h"
#include "AudioLibrary.h"

#ifndef MMAPPER_NO_AUDIO
#include <QSoundEffect>
#endif

#include <QUrl>

SfxManager::SfxManager(const AudioLibrary &library, QObject *const parent)
    : QObject(parent)
    , m_library(library)
{}

void SfxManager::playSound(MAYBE_UNUSED const QString &soundName)
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

    effect->setVolume(static_cast<float>(getConfig().audio.soundVolume) / 100.0f);

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
    float volume = static_cast<float>(getConfig().audio.soundVolume) / 100.0f;
    for (auto *effect : m_activeEffects) {
        effect->setVolume(volume);
    }
#endif
}
