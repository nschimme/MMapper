// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "AudioManager.h"
#include "../configuration/configuration.h"
#ifdef MMAPPER_WITH_MULTIMEDIA
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QSoundEffect>
#endif
#include <QTimer>
#include <QDir>
#include <QUrl>
#include <QDebug>

AudioManager::AudioManager(GameObserver &observer, QObject *parent)
    : QObject(parent)
    , m_observer(observer)
{
#ifdef MMAPPER_WITH_MULTIMEDIA
    m_player = new QMediaPlayer(this);
    m_audioOutput = new QAudioOutput(this);
    m_player->setAudioOutput(m_audioOutput);
    m_player->setLoops(QMediaPlayer::Infinite);

    m_fadeTimer = new QTimer(this);
    m_fadeTimer->setInterval(100);
    connect(m_fadeTimer, &QTimer::timeout, this, [this]() {
        float currentVol = m_audioOutput->volume();
        float target = static_cast<float>(getConfig().audio.musicVolume) / 100.0f;
        if (!getConfig().audio.musicEnabled) {
            target = 0.0f;
        }

        if (m_isFadingOut) {
            currentVol -= m_fadeStep;
            if (currentVol <= 0.0f) {
                currentVol = 0.0f;
                m_fadeTimer->stop();
                m_player->stop();
                if (!m_pendingMusicFile.isEmpty()) {
                    m_currentMusicFile = m_pendingMusicFile;
                    m_pendingMusicFile.clear();
                    if (m_currentMusicFile.startsWith(":")) {
                        m_player->setSource(QUrl("qrc" + m_currentMusicFile));
                    } else {
                        m_player->setSource(QUrl::fromLocalFile(m_currentMusicFile));
                    }
                    if (getConfig().audio.musicEnabled) {
                        m_player->play();
                        startFadeIn();
                    }
                }
            }
        } else {
            currentVol += m_fadeStep;
            if (currentVol >= target) {
                currentVol = target;
                m_fadeTimer->stop();
            }
        }
        m_audioOutput->setVolume(currentVol);
    });
#endif

    m_observer.sig2_areaChanged.connect(m_lifetime, [this](const RoomArea &area) {
        onAreaChanged(area);
    });
    m_observer.sig2_gainedLevel.connect(m_lifetime, [this]() { onGainedLevel(); });
    m_observer.sig2_positionChanged.connect(m_lifetime,
                                            [this](CharacterPositionEnum pos) { onPositionChanged(pos); });

    updateVolumes();
}

AudioManager::~AudioManager()
{
#ifdef MMAPPER_WITH_MULTIMEDIA
    m_player->stop();
#endif
}

void AudioManager::onAreaChanged(const RoomArea &area)
{
    if (area.isEmpty()) {
        m_pendingMusicFile.clear();
        startFadeOut();
        return;
    }

    QString areaName = area.toQString();
    QString musicFile = findAudioFile("music", areaName);

    if (musicFile.isEmpty()) {
        startFadeOut();
        return;
    }

    if (musicFile == m_currentMusicFile) return;

    m_pendingMusicFile = musicFile;
    startFadeOut();
}

void AudioManager::onGainedLevel()
{
    playSound("level_up");
}

void AudioManager::onPositionChanged(CharacterPositionEnum position)
{
    if (position == CharacterPositionEnum::FIGHTING) {
        playSound("combat_start");
    }
}

void AudioManager::updateVolumes()
{
#ifdef MMAPPER_WITH_MULTIMEDIA
    if (!m_isFadingOut && (m_fadeTimer == nullptr || !m_fadeTimer->isActive())) {
        float vol = getConfig().audio.musicEnabled
                        ? static_cast<float>(getConfig().audio.musicVolume) / 100.0f
                        : 0.0f;
        m_audioOutput->setVolume(vol);
    }
#endif
}

void AudioManager::playMusic(const QString &areaName)
{
    QString musicFile = findAudioFile("music", areaName);
    if (musicFile.isEmpty()) return;

    m_pendingMusicFile = musicFile;
    startFadeOut();
}

void AudioManager::playSound(const QString &soundName)
{
#ifdef MMAPPER_WITH_MULTIMEDIA
    if (!getConfig().audio.soundsEnabled) {
        return;
    }

    QString soundFile = findAudioFile("sounds", soundName);
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

QString AudioManager::findAudioFile(const QString &subDir, const QString &name)
{
    QStringList extensions = {"mp3", "wav", "ogg", "m4a"};
    QString resourcesDir = getConfig().canvas.resourcesDirectory;

    // 1. Try disk
    QDir dir(resourcesDir + "/" + subDir);
    for (const auto &ext : extensions) {
        QString fileName = name + "." + ext;
        if (dir.exists(fileName)) {
            return dir.absoluteFilePath(fileName);
        }
        // Try underscore version
        QString underscoreName = QString(name).replace(" ", "_");
        if (dir.exists(underscoreName + "." + ext)) {
            return dir.absoluteFilePath(underscoreName + "." + ext);
        }
    }

    // 2. Try QRC fallback
    for (const auto &ext : extensions) {
        QString qrcPath = QString(":/%1/%2.%3").arg(subDir, name, ext);
        if (QFile::exists(qrcPath)) return qrcPath;

        QString underscoreName = QString(name).replace(" ", "_");
        qrcPath = QString(":/%1/%2.%3").arg(subDir, underscoreName, ext);
        if (QFile::exists(qrcPath)) return qrcPath;
    }

    return "";
}

void AudioManager::startFadeOut()
{
#ifdef MMAPPER_WITH_MULTIMEDIA
    m_isFadingOut = true;
    if (m_fadeTimer && !m_fadeTimer->isActive()) {
        m_fadeTimer->start();
    }
#endif
}

void AudioManager::startFadeIn()
{
#ifdef MMAPPER_WITH_MULTIMEDIA
    m_isFadingOut = false;
    if (m_fadeTimer && !m_fadeTimer->isActive()) {
        m_fadeTimer->start();
    }
#endif
}
