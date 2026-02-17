// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "AudioManager.h"

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

    m_observer.sig2_areaChanged.connect(m_lifetime,
                                        [this](const RoomArea &area) { slot_onAreaChanged(area); });
    m_observer.sig2_gainedLevel.connect(m_lifetime, [this]() { slot_onGainedLevel(); });
    m_observer.sig2_positionChanged.connect(m_lifetime, [this](CharacterPositionEnum pos) {
        slot_onPositionChanged(pos);
    });

    connect(&m_library,
            &MediaLibrary::sig_mediaChanged,
            m_music,
            &MusicManager::slot_onMediaChanged);

    slot_updateVolumes();
}

AudioManager::~AudioManager() = default;

void AudioManager::slot_onAreaChanged(const RoomArea &area)
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

void AudioManager::slot_onGainedLevel()
{
    m_sfx->playSound("level_up");
}

void AudioManager::slot_onPositionChanged(CharacterPositionEnum position)
{
    if (position == CharacterPositionEnum::FIGHTING) {
        m_sfx->playSound("combat_start");
    }
}

void AudioManager::slot_updateVolumes()
{
    m_music->updateVolumes();
    m_sfx->updateVolume();
}
