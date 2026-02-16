// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "AudioManager.h"

#include "../configuration/configuration.h"
#include "../global/Charset.h"
#include "../map/mmapper2room.h"
#include "AudioLibrary.h"
#include "MusicManager.h"
#include "SfxManager.h"

#include <QRegularExpression>

AudioManager::AudioManager(GameObserver &observer, QObject *parent)
    : QObject(parent)
    , m_observer(observer)
{
    m_library = new AudioLibrary(this);
    m_music = new MusicManager(*m_library, this);
    m_sfx = new SfxManager(*m_library, this);

    m_observer.sig2_areaChanged.connect(m_lifetime,
                                        [this](const RoomArea &area) { onAreaChanged(area); });
    m_observer.sig2_gainedLevel.connect(m_lifetime, [this]() { onGainedLevel(); });
    m_observer.sig2_positionChanged.connect(m_lifetime, [this](CharacterPositionEnum pos) {
        onPositionChanged(pos);
    });

    updateVolumes();
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

    QString musicFile = m_library->findAudioFile("music", name);
    m_music->playMusic(musicFile);
}

void AudioManager::onGainedLevel()
{
    m_sfx->playSound("level_up");
}

void AudioManager::onPositionChanged(CharacterPositionEnum position)
{
    if (position == CharacterPositionEnum::FIGHTING) {
        m_sfx->playSound("combat_start");
    }
}

void AudioManager::updateVolumes()
{
    m_music->updateVolumes();
    m_sfx->updateVolume();
}
