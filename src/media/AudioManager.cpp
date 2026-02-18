// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "AudioManager.h"

#include "../configuration/configuration.h"
#include "../global/Charset.h"
#include "../map/mmapper2room.h"
#include "MediaLibrary.h"
#include "MusicManager.h"
#include "SfxManager.h"

#include <QRegularExpression>

AudioManager::AudioManager(MediaLibrary &library, GameObserver &observer, QObject *const parent)
    : QObject(parent)
    , m_library(library)
    , m_observer(observer)
{
    m_music = new MusicManager(m_library, this);
    m_sfx = new SfxManager(m_library, this);

    m_observer.sig2_gainedLevel.connect(m_lifetime, [this]() { m_sfx->playSound("level-up"); });
    connect(&m_library,
            &MediaLibrary::sig_mediaChanged,
            m_music,
            &MusicManager::slot_onMediaChanged);

    m_lastMusicVol = getConfig().audio.musicVolume;
    m_lastSoundVol = getConfig().audio.soundVolume;

    slot_updateVolumes();
}

AudioManager::~AudioManager() = default;

void AudioManager::onAreaChanged(const RoomArea &area)
{
    if (area.isEmpty()) {
        m_music->stopMusic();
        return;
    }

    static const QRegularExpression regex("^the\\s+");
    QString name = area.toQString().toLower().remove(regex).replace(' ', '-');
    mmqt::toAsciiInPlace(name);

    QString musicFile = m_library.findAudio("areas", name);
    m_music->playMusic(musicFile);
}

void AudioManager::playSound(const QString &soundName)
{
    m_sfx->playSound(soundName);
}

void AudioManager::unblockAudio()
{
    if (getConfig().audio.audioHintShown) {
        return;
    }
    setConfig().audio.audioHintShown = true;
    emit sig_audioUnblocked();
    playSound("level-up");
}

void AudioManager::slot_updateVolumes()
{
    const int currentMusicVol = getConfig().audio.musicVolume;
    const int currentSoundVol = getConfig().audio.soundVolume;

    if ((m_lastMusicVol == 0 && currentMusicVol > 0)
        || (m_lastSoundVol == 0 && currentSoundVol > 0)) {
        unblockAudio();
    }

    m_lastMusicVol = currentMusicVol;
    m_lastSoundVol = currentSoundVol;

    m_music->updateVolumes();
    m_sfx->updateVolume();
}
