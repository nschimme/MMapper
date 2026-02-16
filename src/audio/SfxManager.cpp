// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "SfxManager.h"

#include "AudioLibrary.h"
#include "../configuration/configuration.h"

#ifndef MMAPPER_NO_AUDIO
#include <QSoundEffect>
#endif
#include <QUrl>
#include <algorithm>

SfxManager::SfxManager(const AudioLibrary &library, QObject *parent)
    : QObject(parent)
    , m_library(library)
{
}

SfxManager::~SfxManager() = default;

void SfxManager::playSound([[maybe_unused]] const QString &soundName)
{
#ifndef MMAPPER_NO_AUDIO
    if (!getConfig().audio.enabled || !getConfig().audio.soundEffectsEnabled) {
        return;
    }

    QString path = m_library.findAudioFile("sounds", soundName);
    if (path.isEmpty()) {
        return;
    }

    // Clean up finished effects first
    m_activeEffects.erase(
        std::remove_if(m_activeEffects.begin(), m_activeEffects.end(),
                       [](const auto &e) { return !e->isPlaying() && e->status() != QSoundEffect::Loading; }),
        m_activeEffects.end());

    auto effect = std::make_unique<QSoundEffect>();
    if (path.startsWith(":/")) {
        effect->setSource(QUrl(QStringLiteral("qrc") + path));
    } else {
        effect->setSource(QUrl::fromLocalFile(path));
    }

    effect->setVolume(getConfig().audio.soundEffectsVolume / 100.0f);
    effect->play();

    m_activeEffects.emplace_back(std::move(effect));
#endif
}

void SfxManager::updateVolume()
{
#ifndef MMAPPER_NO_AUDIO
    // Clean up finished effects
    m_activeEffects.erase(
        std::remove_if(m_activeEffects.begin(), m_activeEffects.end(),
                       [](const auto &e) { return !e->isPlaying() && e->status() != QSoundEffect::Loading; }),
        m_activeEffects.end());

    float volume = getConfig().audio.soundEffectsVolume / 100.0f;
    for (auto &effect : m_activeEffects) {
        effect->setVolume(volume);
    }
#endif
}
