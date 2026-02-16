// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "AudioManager.h"

#include "../configuration/configuration.h"
#include "../global/Charset.h"
#include "../map/mmapper2room.h"

#ifndef MMAPPER_NO_AUDIO
#include <QAudioOutput>
#include <QMediaPlayer>
#include <QSoundEffect>
#endif
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>

AudioManager::AudioManager(GameObserver &observer, QObject *parent)
    : QObject(parent)
    , m_observer(observer)
{
#ifndef MMAPPER_NO_AUDIO
    for (int i = 0; i < 2; ++i) {
        m_music.channels[i].player = new QMediaPlayer(this);
        m_music.channels[i].audioOutput = new QAudioOutput(this);
        m_music.channels[i].player->setAudioOutput(m_music.channels[i].audioOutput);
        m_music.channels[i].player->setLoops(QMediaPlayer::Infinite);

        connect(m_music.channels[i].player,
                &QMediaPlayer::mediaStatusChanged,
                this,
                [this, i](QMediaPlayer::MediaStatus status) {
                    if (status == QMediaPlayer::LoadedMedia || status == QMediaPlayer::BufferedMedia
                        || status == QMediaPlayer::BufferingMedia) {
                        applyPendingPosition(i);
                    }
                });

        connect(m_music.channels[i].player,
                &QMediaPlayer::seekableChanged,
                this,
                [this, i](bool seekable) {
                    if (seekable) {
                        applyPendingPosition(i);
                    }
                });
    }

    m_fade.step = static_cast<float>(FadeState::FADE_INTERVAL_MS)
                  / FadeState::CROSSFADE_DURATION_MS;

    m_fade.timer = new QTimer(this);
    m_fade.timer->setInterval(FadeState::FADE_INTERVAL_MS);
    connect(m_fade.timer, &QTimer::timeout, this, [this]() {
        bool changed = false;
        for (int i = 0; i < 2; ++i) {
            float target = 0.0f;
            if (!m_fade.fadingToSilence && i == m_music.activeChannel) {
                target = 1.0f;
            }

            if (m_music.channels[i].fadeVolume < target) {
                m_music.channels[i].fadeVolume = std::min(target,
                                                          m_music.channels[i].fadeVolume
                                                              + m_fade.step);
                changed = true;
            } else if (m_music.channels[i].fadeVolume > target) {
                m_music.channels[i].fadeVolume = std::max(target,
                                                          m_music.channels[i].fadeVolume
                                                              - m_fade.step);
                changed = true;
            }
            updateChannelVolume(i);
        }

        if (!changed) {
            m_fade.timer->stop();
            int inactive = (m_music.activeChannel + 1) % 2;
            if (m_music.channels[inactive].fadeVolume <= 0.0f) {
                if (!m_music.channels[inactive].file.isEmpty()) {
                    if (m_music.channels[inactive].player->playbackState()
                        == QMediaPlayer::PlayingState) {
                        m_music.cachedPositions
                            .insert(m_music.channels[inactive].file,
                                    new qint64(m_music.channels[inactive].player->position()));
                    }
                    m_music.channels[inactive].player->stop();
                    m_music.channels[inactive].file.clear();
                }
            }
            if (m_fade.fadingToSilence
                && m_music.channels[m_music.activeChannel].fadeVolume <= 0.0f) {
                if (m_music.channels[m_music.activeChannel].player->playbackState()
                    == QMediaPlayer::PlayingState) {
                    m_music.cachedPositions
                        .insert(m_music.channels[m_music.activeChannel].file,
                                new qint64(
                                    m_music.channels[m_music.activeChannel].player->position()));
                }
                m_music.channels[m_music.activeChannel].player->stop();
                m_music.channels[m_music.activeChannel].file.clear();
            }
        }
    });

    scanDirectories();

    const auto &resourcesDir = getConfig().canvas.resourcesDirectory;
    m_resources.watcher.addPath(resourcesDir + "/music");
    m_resources.watcher.addPath(resourcesDir + "/sounds");
    connect(&m_resources.watcher,
            &QFileSystemWatcher::directoryChanged,
            this,
            [this](const QString & /*path*/) { scanDirectories(); });
#endif

    m_observer.sig2_areaChanged.connect(m_lifetime,
                                        [this](const RoomArea &area) { onAreaChanged(area); });
    m_observer.sig2_gainedLevel.connect(m_lifetime, [this]() { onGainedLevel(); });
    m_observer.sig2_positionChanged.connect(m_lifetime, [this](CharacterPositionEnum pos) {
        onPositionChanged(pos);
    });

    updateVolumes();
}

AudioManager::~AudioManager()
{
#ifndef MMAPPER_NO_AUDIO
    for (int i = 0; i < 2; ++i) {
        m_music.channels[i].player->stop();
    }
#endif
}

void AudioManager::onAreaChanged(const RoomArea &area)
{
#ifndef MMAPPER_NO_AUDIO
    if (area.isEmpty()) {
        startFade(true);
        return;
    }

    static const QRegularExpression regex("^the\\s+");
    QString name = area.toQString().toLower().remove(regex).replace(' ', '-');
    mmqt::toAsciiInPlace(name);

    QString musicFile = findAudioFile("music", name);

    if (musicFile.isEmpty()) {
        startFade(true);
        return;
    }

    if (musicFile == m_music.channels[m_music.activeChannel].file) {
        startFade(false);
        return;
    }

    int inactiveChannel = (m_music.activeChannel + 1) % 2;
    if (musicFile == m_music.channels[inactiveChannel].file) {
        m_music.activeChannel = inactiveChannel;
        startFade(false);
        return;
    }

    // New file: start crossfade
    // 1. Save current active channel position
    auto &active = m_music.channels[m_music.activeChannel];
    if (!active.file.isEmpty() && active.player->playbackState() == QMediaPlayer::PlayingState) {
        m_music.cachedPositions.insert(active.file, new qint64(active.player->position()));
    }

    // 2. Prepare inactive channel
    m_music.activeChannel = inactiveChannel;
    auto &newActive = m_music.channels[m_music.activeChannel];
    newActive.file = musicFile;
    newActive.fadeVolume = 0.0f;

    if (qint64 *pos = m_music.cachedPositions.object(newActive.file)) {
        newActive.pendingPosition = *pos;
    } else {
        newActive.pendingPosition = -1;
    }

    if (newActive.file.startsWith(":")) {
        newActive.player->setSource(QUrl("qrc" + newActive.file));
    } else {
        newActive.player->setSource(QUrl::fromLocalFile(newActive.file));
    }

    if (getConfig().audio.musicVolume > 0) {
        newActive.player->play();
        applyPendingPosition(m_music.activeChannel);
    }

    startFade(false);
#else
    Q_UNUSED(area);
#endif
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
#ifndef MMAPPER_NO_AUDIO
    for (int i = 0; i < 2; ++i) {
        updateChannelVolume(i);
    }

    float masterVol = static_cast<float>(getConfig().audio.musicVolume) / 100.0f;
    auto &active = m_music.channels[m_music.activeChannel];
    if (masterVol > 0 && active.player->playbackState() == QMediaPlayer::StoppedState
        && !active.file.isEmpty()) {
        if (qint64 *pos = m_music.cachedPositions.object(active.file)) {
            active.pendingPosition = *pos;
        }
        active.player->play();
        applyPendingPosition(m_music.activeChannel);
    } else if (masterVol <= 0 && active.player->playbackState() == QMediaPlayer::PlayingState) {
        if (!active.file.isEmpty()) {
            m_music.cachedPositions.insert(active.file, new qint64(active.player->position()));
        }
        active.player->stop();
    }
#endif
}

#ifndef MMAPPER_NO_AUDIO
void AudioManager::applyPendingPosition(int channelIndex)
{
    auto &ch = m_music.channels[channelIndex];
    if (ch.pendingPosition != -1 && ch.player->isSeekable()) {
        ch.player->setPosition(ch.pendingPosition);
        ch.pendingPosition = -1;
    }
}

void AudioManager::updateChannelVolume(int channelIndex)
{
    float masterVol = static_cast<float>(getConfig().audio.musicVolume) / 100.0f;
    m_music.channels[channelIndex].audioOutput->setVolume(
        masterVol * m_music.channels[channelIndex].fadeVolume);
}
#endif

void AudioManager::playMusic(const QString &areaName)
{
#ifndef MMAPPER_NO_AUDIO
    QString musicFile = findAudioFile("music", areaName);
    if (musicFile.isEmpty())
        return;

    // Use onAreaChanged logic via a dummy RoomArea or similar if needed,
    // but here we can just do the crossfade logic.
    // Actually, onAreaChanged handles it well.
    onAreaChanged(RoomArea(areaName.toStdString()));
#else
    Q_UNUSED(areaName);
#endif
}

void AudioManager::playSound(const QString &soundName)
{
    Q_UNUSED(soundName);
#ifndef MMAPPER_NO_AUDIO
    if (getConfig().audio.soundVolume <= 0) {
        return;
    }

    QString name = soundName.toLower().replace(' ', '-');
    QString soundFile = findAudioFile("sounds", name);
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
#endif
}

QString AudioManager::findAudioFile(const QString &subDir, const QString &name)
{
    QString key = subDir + "/" + name;
    auto it = m_resources.availableFiles.find(key);
    if (it != m_resources.availableFiles.end()) {
        return it.value();
    }
    return "";
}

void AudioManager::scanDirectories()
{
    m_resources.availableFiles.clear();
    QStringList extensions = {"mp3", "wav", "ogg", "m4a"};

    auto scanPath = [&](const QString &path) {
        QDirIterator it(path, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QFileInfo fileInfo(it.next());
            QString suffix = fileInfo.suffix().toLower();

            if (extensions.contains(suffix)) {
                QString filePath = fileInfo.filePath();
                auto dotIndex = filePath.lastIndexOf('.');
                if (dotIndex == -1) {
                    continue;
                }

                const bool isQrc = filePath.startsWith(":/");
                QString baseName;
                if (isQrc) {
                    baseName = filePath.left(dotIndex).mid(2); // remove :/
                } else {
                    QString resourcesPath = getConfig().canvas.resourcesDirectory;
                    if (!filePath.startsWith(resourcesPath)) {
                        continue;
                    }
                    baseName = filePath.mid(resourcesPath.length())
                                   .left(dotIndex - resourcesPath.length());
                    if (baseName.startsWith("/")) {
                        baseName = baseName.mid(1);
                    }
                }

                if (baseName.isEmpty()) {
                    continue;
                }

                m_resources.availableFiles.insert(baseName, filePath);
            }
        }
    };

    // Scan QRC first then disk to prioritize disk
    scanPath(":/music");
    scanPath(":/sounds");

    const auto &resourcesDir = getConfig().canvas.resourcesDirectory;
    scanPath(resourcesDir + "/music");
    scanPath(resourcesDir + "/sounds");

    qInfo() << "Scanned audio directories. Found" << m_resources.availableFiles.size() << "files.";
}

void AudioManager::startFade(bool toSilence)
{
#ifndef MMAPPER_NO_AUDIO
    m_fade.fadingToSilence = toSilence;
    if (m_fade.timer && !m_fade.timer->isActive()) {
        m_fade.timer->start();
    }
#endif
}
